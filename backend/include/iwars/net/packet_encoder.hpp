#pragma once

#include "iwars/sim/entity.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace iwars {

class PacketEncoder {
 public:
  virtual ~PacketEncoder() = default;
  virtual std::vector<std::uint8_t> encode(const std::vector<Entity>& entities,
                                           double sim_time_s) const = 0;
};

/** Placeholder radar-stim truth protocol (IWP2).
 *  Replace with real DSS/radar packet when format arrives.
 *
 * Layout — fixed BIG-ENDIAN (network byte order) on the wire so
 * Solaris DSS and Linux DSS decode the same bytes.
 *   magic[4] = 'I','W','P','2'
 *   version  u16 BE = 2
 *   count    u16 BE
 *   sim_time f64 BE
 *   per entity:
 *     id, affiliation, platform, iff_mode, squawk  (u16 BE len + utf8 each)
 *     lat f64 BE, lon f64 BE, alt_m f64 BE
 *     heading_deg f64 BE, speed_mps f64 BE, climb_mps f64 BE
 *     iff_enabled u8, mode_c u8, mode_4 u8, mode_5 u8
 */
class PlaceholderEncoder : public PacketEncoder {
 public:
  std::vector<std::uint8_t> encode(const std::vector<Entity>& entities,
                                   double sim_time_s) const override;
};

}  // namespace iwars
