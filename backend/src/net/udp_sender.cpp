#include "iwars/net/udp_sender.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace iwars {

// BU: Default to the placeholder IWP2 encoder; socket is opened lazily on first send.
// BU: ICD swap: replace PlaceholderEncoder with the DSS encoder class here (or call set_encoder from main).
UdpSender::UdpSender() : encoder_(std::make_unique<PlaceholderEncoder>()) {}

// BU: Release the datagram socket if configure()/send() opened one.
UdpSender::~UdpSender() { close_socket(); }

void UdpSender::configure(UdpConfig cfg) {
  std::lock_guard lock(mu_);  // BU: cfg_ and sock_ are shared with send().
  cfg_ = std::move(cfg);      // BU: Store host/port/enabled.
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

  const auto payload = encoder_->encode(entities, sim_time_s);  // BU: ICD lives in encode(); send() only ships the bytes.

  sockaddr_in addr{};                          // BU: IPv4 destination filled below.
  addr.sin_family = AF_INET;                   // BU: IPv4.
  addr.sin_port = htons(static_cast<uint16_t>(cfg_.port));  // BU: Port in network byte order.
  if (::inet_pton(AF_INET, cfg_.host.c_str(), &addr.sin_addr) != 1) {  // BU: Parse dotted-quad host.
    return false;                              // BU: Bad host string — do not send.
  }

  const auto n = ::sendto(sock_, payload.data(), payload.size(), 0,  // BU: Fire-and-forget datagram.
                          reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  return n == static_cast<ssize_t>(payload.size());  // BU: True only if the full payload was queued.
}

}  // namespace iwars
