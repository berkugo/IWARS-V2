#include "iwars/net/udp_sender.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace iwars {
namespace {

bool send_to(int sock, const std::string& host, int port,
             const std::vector<std::uint8_t>& payload) {
  if (sock < 0 || payload.empty()) return false;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) return false;
  const auto n = ::sendto(sock, payload.data(), payload.size(), 0,
                          reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  return n == static_cast<ssize_t>(payload.size());
}

}  // namespace

// BU: Default to the placeholder IWP2 encoder; socket is opened lazily on first send.
// BU: ICD swap: replace PlaceholderEncoder with the DSS encoder class here (or call set_encoder from main).
UdpSender::UdpSender() : encoder_(std::make_unique<PlaceholderEncoder>()) {}

// BU: Release the datagram socket if configure()/send() opened one.
UdpSender::~UdpSender() { close_socket(); }

void UdpSender::configure(UdpConfig cfg) {
  std::lock_guard lock(mu_);  // BU: cfg_ and sock_ are shared with send().
  normalize_udp(cfg);         // BU: Force localhost; sync legacy port with entity_port.
  cfg_ = std::move(cfg);      // BU: Store host/ports/enabled.
  close_socket();             // BU: Drop the old fd so the next send uses a fresh socket (and new dest).
}

UdpConfig UdpSender::config() const {
  std::lock_guard lock(mu_);  // BU: Return a copy so callers do not race configure().
  return cfg_;                // BU: Current UDP settings.
}

void UdpSender::set_encoder(std::unique_ptr<PacketEncoder> encoder) {
  std::lock_guard lock(mu_);          // BU: Encoder is used by send() under the same lock.
  encoder_ = std::move(encoder);      // BU: Swap in a real DSS encoder when the ICD arrives.
}

bool UdpSender::ensure_socket() {
  if (sock_ >= 0) return true;                 // BU: Already open.
  sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);    // BU: IPv4 UDP socket (unconnected; sendto sets dest).
  return sock_ >= 0;                           // BU: False if the OS refused the socket.
}

void UdpSender::close_socket() {
  if (sock_ >= 0) {        // BU: Only close a real fd.
    ::close(sock_);        // BU: Release the kernel socket.
    sock_ = -1;            // BU: Mark closed so ensure_socket() recreates it.
  }
}

bool UdpSender::send(const std::vector<Entity>& entities, double sim_time_s) {
  std::lock_guard lock(mu_);                   // BU: Serialize with configure()/set_encoder().
  if (!cfg_.enabled) return false;             // BU: UI/scenario turned the feed off.
  if (!encoder_) return false;                 // BU: No format installed.
  if (!ensure_socket()) return false;          // BU: Could not open a UDP socket.

  UdpConfig cfg = cfg_;
  normalize_udp(cfg);

  std::vector<Entity> own;
  std::vector<Entity> others;
  own.reserve(1);
  others.reserve(entities.size());
  for (const auto& e : entities) {
    if (is_ownship(e)) own.push_back(e);
    else others.push_back(e);
  }

  bool ok = true;
  if (!own.empty()) {
    const auto payload = encoder_->encode(own, sim_time_s);
    ok = send_to(sock_, cfg.host, cfg.ownship_port, payload) && ok;
  }
  if (!others.empty()) {
    const auto payload = encoder_->encode(others, sim_time_s);
    ok = send_to(sock_, cfg.host, cfg.entity_port, payload) && ok;
  }
  return ok;
}

}  // namespace iwars
