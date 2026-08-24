#pragma once

#include "iwars/net/packet_encoder.hpp"
#include "iwars/sim/scenario.hpp"

#include <memory>
#include <mutex>
#include <string>

namespace iwars {

// BU: Owns a UDP datagram socket and an encoder; sends truth packets when enabled.
class UdpSender {
 public:
  UdpSender();   // BU: Construct with the placeholder IWP2 encoder and no open socket yet.
  ~UdpSender();  // BU: Close the socket if it was opened.

  UdpSender(const UdpSender&) = delete;             // BU: Not copyable — unique socket fd + encoder.
  UdpSender& operator=(const UdpSender&) = delete;  // BU: Not assignable for the same reason.

  void configure(UdpConfig cfg);  // BU: Replace host/port/enabled and drop the old socket so the next send rebinds.
  UdpConfig config() const;       // BU: Locked copy of the current UDP settings.

  void set_encoder(std::unique_ptr<PacketEncoder> encoder);  // BU: ICD swap point — pass a DSS encoder instead of PlaceholderEncoder.
  bool send(const std::vector<Entity>& entities, double sim_time_s);  // BU: One datagram per call (whole air picture). Split here if DSS is per-track.

 private:
  mutable std::mutex mu_;                       // BU: Guards cfg_, encoder_, and sock_.
  UdpConfig cfg_;                               // BU: Current destination and enable flag.
  std::unique_ptr<PacketEncoder> encoder_;      // BU: Active packet format (default PlaceholderEncoder).
  int sock_{-1};                                // BU: UDP socket fd, or -1 if not yet created.
  bool ensure_socket();                         // BU: Lazy-create AF_INET SOCK_DGRAM if sock_ is closed.
  void close_socket();                          // BU: close(2) and reset sock_ to -1.
};

}  // namespace iwars
