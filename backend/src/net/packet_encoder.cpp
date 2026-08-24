#include "iwars/net/packet_encoder.hpp"

#include <cstring>

namespace iwars {
namespace {

/** Wire format is fixed big-endian (network byte order) so Solaris DSS and
 *  Linux DSS decode identically regardless of host CPU endianness.
 */

void append_u16_be(std::vector<std::uint8_t>& out, std::uint16_t v) {
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
  out.push_back(static_cast<std::uint8_t>(v & 0xff));
}

void append_u8(std::vector<std::uint8_t>& out, std::uint8_t v) { out.push_back(v); }

void append_f64_be(std::vector<std::uint8_t>& out, double v) {
  std::uint8_t host[8];
  std::memcpy(host, &v, 8);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  out.insert(out.end(), host, host + 8);
#else
  // Host is little-endian (typical x86_64 Linux) — swap to BE on the wire
  for (int i = 7; i >= 0; --i) out.push_back(host[i]);
#endif
}

void append_str(std::vector<std::uint8_t>& out, const std::string& s) {
  append_u16_be(out, static_cast<std::uint16_t>(s.size()));
  out.insert(out.end(), s.begin(), s.end());
}

}  // namespace

std::vector<std::uint8_t> PlaceholderEncoder::encode(
    const std::vector<Entity>& entities, double sim_time_s) const {
  std::vector<std::uint8_t> out;
  out.reserve(64 + entities.size() * 160);
  out.push_back('I');
  out.push_back('W');
  out.push_back('P');
  out.push_back('2');
  append_u16_be(out, 2);
  append_u16_be(out, static_cast<std::uint16_t>(entities.size()));
  append_f64_be(out, sim_time_s);
  for (const auto& e : entities) {
    append_str(out, e.id);
    append_str(out, e.affiliation);
    append_str(out, e.platform);
    append_str(out, e.iff_mode);
    append_str(out, e.squawk);
    append_f64_be(out, e.lat);
    append_f64_be(out, e.lon);
    append_f64_be(out, e.alt_m);
    append_f64_be(out, e.heading_deg);
    append_f64_be(out, e.speed_mps);
    append_f64_be(out, e.climb_mps);
    append_u8(out, e.iff_enabled ? 1 : 0);
    append_u8(out, e.mode_c ? 1 : 0);
    append_u8(out, e.mode_4 ? 1 : 0);
    append_u8(out, e.mode_5 ? 1 : 0);
  }
  return out;
}

}  // namespace iwars
