#pragma once

#include "iwars/net/packet_encoder.hpp"
#include "iwars/sim/scenario.hpp"

#include <memory>
#include <mutex>
#include <string>

namespace iwars {

class UdpSender {
 public:
  UdpSender();
  ~UdpSender();

  UdpSender(const UdpSender&) = delete;
  UdpSender& operator=(const UdpSender&) = delete;

  void configure(UdpConfig cfg);
  UdpConfig config() const;

  void set_encoder(std::unique_ptr<PacketEncoder> encoder);
  bool send(const std::vector<Entity>& entities, double sim_time_s);

 private:
  mutable std::mutex mu_;
  UdpConfig cfg_;
  std::unique_ptr<PacketEncoder> encoder_;
  int sock_{-1};
  bool ensure_socket();
  void close_socket();
};

}  // namespace iwars
