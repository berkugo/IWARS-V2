#pragma once

#include "iwars/sim/entity.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace iwars {

// BU: Strategy interface: turn the current entity list + sim time into a UDP datagram.
class PacketEncoder {
 public:
  virtual ~PacketEncoder() = default;  // BU: Virtual dtor so unique_ptr<PacketEncoder> deletes the subclass.
  virtual std::vector<std::uint8_t> encode(const std::vector<Entity>& entities,
                                           double sim_time_s) const = 0;  // BU: Produce wire bytes for one tick.
};

/*
 * =============================================================================
 * DSS / radar-stim ICD swap (read this before changing the UDP encoder)
 * =============================================================================
 *
 * Tomorrow: feed the DSS / workplace-radar source to an AI, extract the
 * datagram(s) it actually parses, then implement THAT layout here.
 *
 * What IWARS is:
 *   Entity-truth publisher. The engine already has lat/lon/alt, heading,
 *   ground speed (m/s), climb (m/s), affiliation, platform, IFF flags,
 *   Mode 3/A squawk, callsign, ownship flag. See Entity in entity.hpp.
 *   At 10 Hz while PLAYING, main.cpp calls UdpSender::send(entities, sim_time).
 *
 * What to change (minimal set):
 *   1. THIS FILE + packet_encoder.cpp
 *        Replace PlaceholderEncoder::encode() OR add a new class (e.g. DssEncoder)
 *        that implements PacketEncoder and pack the DSS fields in the DSS
 *        endianness / units / scaling. Wire UdpSender to that class.
 *   2. udp_sender.cpp
 *        UdpSender() currently does make_unique<PlaceholderEncoder>().
 *        Point that at the new encoder. Change send() only if DSS wants
 *        one datagram PER entity, a different socket type, or TCP.
 *   3. main.cpp (engine.set_state_callback)
 *        Today UDP is emitted only when state["playing"] is true, one
 *        datagram for the whole picture. Change that loop only if DSS
 *        needs paused heartbeats, a different rate, or per-track packets.
 *   4. entity.hpp Entity
 *        Add members ONLY if DSS needs fields IWARS does not have yet
 *        (ICAO address, Mode 1/2 codes, RCS, emitter list, …). Then
 *        thread them through to_json/from_json and the UI.
 *
 * What NOT to change for a format-only ICD:
 *   Engine kinematics, HTTP/WS JSON, React UI, scenario files.
 *
 * Units on Entity today (convert in encode() if DSS wants something else):
 *   lat, lon        decimal degrees, WGS-ish spherical
 *   alt_m           meters MSL
 *   heading_deg     true heading, 0 = north, 90 = east
 *   speed_mps       ground speed meters/second (UI shows knots)
 *   climb_mps       vertical rate m/s, positive = climb
 *   squawk          Mode 3/A as a string (e.g. "1200"), not packed octal
 *   iff_mode        "off"|"1"|"2"|"3A"|"C"|"S"|"4"|"5"
 *   affiliation     friend|assumed_friend|neutral|suspect|hostile|unknown
 *   platform        fighter|attack|bomber|transport|tanker|aew|isr|uav|
 *                   ucav|helicopter|civil|unknown
 *   ownship         true only for AEWC737 (id "ownship")
 *
 * PlaceholderEncoder below is NOT the DSS ICD. It is a stand-in (IWP2)
 * so the UDP path can be tested. Delete or keep it as a debug encoder.
 * =============================================================================
 */

/** Placeholder radar-stim truth protocol (IWP2) — NOT the real DSS format.
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
// BU: Temporary IWP2 encoder used until the real radar-stim ICD is plugged in.
class PlaceholderEncoder : public PacketEncoder {
 public:
  std::vector<std::uint8_t> encode(const std::vector<Entity>& entities,
                                   double sim_time_s) const override;  // BU: Pack header + every entity into BE bytes.
};

}  // namespace iwars
