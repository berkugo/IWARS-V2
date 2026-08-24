#include "iwars/net/udp_sender.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace iwars {

UdpSender::UdpSender() : encoder_(std::make_unique<PlaceholderEncoder>()) {}

UdpSender::~UdpSender() { close_socket(); }

void UdpSender::configure(UdpConfig cfg) {
  std::lock_guard lock(mu_);
  cfg_ = std::move(cfg);
  close_socket();
}

UdpConfig UdpSender::config() const {
  std::lock_guard lock(mu_);
  return cfg_;
}

void UdpSender::set_encoder(std::unique_ptr<PacketEncoder> encoder) {
  std::lock_guard lock(mu_);
  encoder_ = std::move(encoder);
}

bool UdpSender::ensure_socket() {
  if (sock_ >= 0) return true;
  sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  return sock_ >= 0;
}

void UdpSender::close_socket() {
  if (sock_ >= 0) {
    ::close(sock_);
    sock_ = -1;
  }
}

bool UdpSender::send(const std::vector<Entity>& entities, double sim_time_s) {
  std::lock_guard lock(mu_);
  if (!cfg_.enabled) return false;
  if (!encoder_) return false;
  if (!ensure_socket()) return false;

  const auto payload = encoder_->encode(entities, sim_time_s);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(cfg_.port));
  if (::inet_pton(AF_INET, cfg_.host.c_str(), &addr.sin_addr) != 1) {
    return false;
  }

  const auto n = ::sendto(sock_, payload.data(), payload.size(), 0,
                          reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  return n == static_cast<ssize_t>(payload.size());
}

}  // namespace iwars
