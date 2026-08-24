#include "iwars/net/packet_encoder.hpp"

#include <cstring>

namespace iwars {
namespace {

/** Wire format is fixed big-endian (network byte order) so Solaris DSS and
 *  Linux DSS decode identically regardless of host CPU endianness.
 */

// BU: Append a 16-bit integer as two bytes, most-significant first.
void append_u16_be(std::vector<std::uint8_t>& out, std::uint16_t v) {
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));  // BU: High byte.
  out.push_back(static_cast<std::uint8_t>(v & 0xff));         // BU: Low byte.
}

// BU: Append a single byte (flags, etc.).
void append_u8(std::vector<std::uint8_t>& out, std::uint8_t v) { out.push_back(v); }

// BU: Append an IEEE-754 double in big-endian byte order.
void append_f64_be(std::vector<std::uint8_t>& out, double v) {
  std::uint8_t host[8];                 // BU: Local copy of the 8 IEEE bytes in host order.
  std::memcpy(host, &v, 8);             // BU: Type-pun the double into bytes without UB via memcpy.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  out.insert(out.end(), host, host + 8);  // BU: Already BE (e.g. SPARC) — copy as-is.
#else
  // Host is little-endian (typical x86_64 Linux) — swap to BE on the wire
  for (int i = 7; i >= 0; --i) out.push_back(host[i]);  // BU: Reverse bytes for network order.
#endif
}

// BU: Length-prefixed UTF-8 string: u16 BE length, then raw bytes (no NUL).
void append_str(std::vector<std::uint8_t>& out, const std::string& s) {
  append_u16_be(out, static_cast<std::uint16_t>(s.size()));  // BU: Byte length of the payload.
  out.insert(out.end(), s.begin(), s.end());                 // BU: UTF-8 octets.
}

}  // namespace

std::vector<std::uint8_t> PlaceholderEncoder::encode(
    const std::vector<Entity>& entities, double sim_time_s) const {
  std::vector<std::uint8_t> out;                         // BU: Datagram we will sendto().
  out.reserve(64 + entities.size() * 160);               // BU: Avoid reallocs for a typical air picture.
  out.push_back('I');                                    // BU: Magic[0] — IWP2 identifier.
  out.push_back('W');                                    // BU: Magic[1].
  out.push_back('P');                                    // BU: Magic[2].
  out.push_back('2');                                    // BU: Magic[3].
  append_u16_be(out, 2);                                 // BU: Protocol version = 2.
  append_u16_be(out, static_cast<std::uint16_t>(entities.size()));  // BU: Number of entity records that follow.
  append_f64_be(out, sim_time_s);                        // BU: Simulation time in seconds.
  for (const auto& e : entities) {                       // BU: One record per track, including ownship.
    append_str(out, e.id);                               // BU: Track id.
    append_str(out, e.affiliation);                      // BU: friend/hostile/….
    append_str(out, e.platform);                         // BU: fighter/aew/uav/….
    append_str(out, e.iff_mode);                         // BU: IFF mode string.
    append_str(out, e.squawk);                           // BU: Mode 3/A code.
    append_f64_be(out, e.lat);                           // BU: Latitude degrees.
    append_f64_be(out, e.lon);                           // BU: Longitude degrees.
    append_f64_be(out, e.alt_m);                         // BU: Altitude meters MSL.
    append_f64_be(out, e.heading_deg);                   // BU: Heading degrees.
    append_f64_be(out, e.speed_mps);                     // BU: Ground speed m/s.
    append_f64_be(out, e.climb_mps);                     // BU: Vertical rate m/s.
    append_u8(out, e.iff_enabled ? 1 : 0);               // BU: IFF master flag.
    append_u8(out, e.mode_c ? 1 : 0);                    // BU: Mode C flag.
    append_u8(out, e.mode_4 ? 1 : 0);                    // BU: Mode 4 flag.
    append_u8(out, e.mode_5 ? 1 : 0);                    // BU: Mode 5 flag.
  }
  return out;                                            // BU: Complete datagram bytes.
}

}  // namespace iwars
