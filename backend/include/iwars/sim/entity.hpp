#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace iwars {

// BU: Stable id for the AEW ownship; used to block delete and to match loaded JSON.
inline constexpr const char* kOwnshipId = "ownship";
// BU: Display callsign painted on the map and in the track list (AEWC737).
inline constexpr const char* kOwnshipCallsign = "AEWC737";

// BU: One lat/lon vertex on an entity's waypoint route.
struct Waypoint {
  double lat{0};  // BU: Waypoint latitude in decimal degrees.
  double lon{0};  // BU: Waypoint longitude in decimal degrees.
};

/** Truth target for radar/IFF stimulators.
 *  Downstream radar sim turns these into radar blips + IFF replies.
 */
// BU: One air track's identity, pose, kinematics, IFF, and optional route.
struct Entity {
  std::string id;    // BU: Unique track id used by the REST API and UDP payload.
  std::string name;  // BU: Callsign / track label shown in the UI (e.g. VIPER1).

  // friend | assumed_friend | neutral | suspect | hostile | unknown
  std::string affiliation{"friend"};  // BU: NATO-style identity used for icon color.

  // Air C4ISR: fighter | attack | bomber | transport | tanker | aew | isr |
  // uav | ucav | helicopter | civil | unknown  (+ legacy aircraft)
  std::string platform{"fighter"};  // BU: Platform class used for iconography and encoding.

  double lat{0};           // BU: Current geodetic latitude (degrees).
  double lon{0};           // BU: Current geodetic longitude (degrees).
  double alt_m{0};         // BU: MSL altitude in meters (truth + Mode-C source).
  double heading_deg{0};   // BU: True heading; 0 = north, 90 = east.
  double speed_mps{0};     // BU: Ground speed in meters per second.
  double climb_mps{0};     // BU: Vertical rate in meters per second (positive = climb).

  // IFF / SIF (air C4ISR)
  bool iff_enabled{true};          // BU: Whether this track answers IFF interrogations.
  std::string iff_mode{"3A"};      // BU: Active IFF mode: off | 1 | 2 | 3A | C | S | 4 | 5.
  std::string squawk{"1200"};      // BU: Mode 3/A octal code stored as a string.
  bool mode_c{true};               // BU: Report altitude via Mode C when interrogated.
  bool mode_4{false};              // BU: Crypto friend-reply stim flag.
  bool mode_5{false};              // BU: Mode 5 stim flag.
  bool ownship{false};             // BU: True for the fixed AEW ownship (AEWC737).

  std::vector<Waypoint> route;     // BU: Optional waypoint list the engine steers toward.
  std::size_t route_index{0};      // BU: Index of the next waypoint to fly to.

  // Legacy aliases kept in JSON for older scenarios
  // side -> affiliation, entity_type -> platform, alt -> alt_m
};

// BU: True if this entity is the protected AEWC737 ownship by flag, id, or callsign.
inline bool is_ownship(const Entity& e) {
  return e.ownship || e.id == kOwnshipId || e.name == kOwnshipCallsign;
}

/** B737 AEW&C ownship — always present; kinematics are operator-editable. */
// BU: Factory for a default AEWC737 sitting at the given map center, FL312, heading east.
inline Entity make_ownship(double lat, double lon) {
  Entity e;                         // BU: Start from default-constructed zeros / defaults.
  e.id = kOwnshipId;                // BU: Force the canonical ownship id.
  e.name = kOwnshipCallsign;        // BU: Force the AEWC737 callsign.
  e.affiliation = "friend";         // BU: Ownship is always friendly.
  e.platform = "aew";               // BU: Ownship is an AEW platform.
  e.lat = lat;                      // BU: Place it at the requested latitude.
  e.lon = lon;                      // BU: Place it at the requested longitude.
  e.alt_m = 9500;                   // BU: Default cruise altitude ~FL312.
  e.heading_deg = 90;               // BU: Default heading east.
  e.speed_mps = 0;                  // BU: Default parked; operator may later set a cruise speed.
  e.iff_enabled = true;             // BU: Ownship replies to IFF.
  e.iff_mode = "3A";                // BU: Default Mode 3/A.
  e.squawk = "0001";                // BU: Distinct ownship squawk.
  e.mode_c = true;                  // BU: Report Mode C altitude.
  e.mode_4 = true;                  // BU: Crypto friend reply enabled for ownship.
  e.mode_5 = true;                  // BU: Mode 5 enabled for ownship.
  e.ownship = true;                 // BU: Mark as the protected ownship.
  e.route.clear();                  // BU: Default empty route; operator may add waypoints later.
  e.route_index = 0;                // BU: Start at the first waypoint if a route is later added.
  return e;                         // BU: Return the fully specified ownship entity.
}

/** Force AEWC737 identity; pose / speed / IFF remain operator-editable. */
inline void normalize_ownship(Entity& e) {
  e.id = kOwnshipId;                // BU: Restore canonical id.
  e.name = kOwnshipCallsign;        // BU: Restore AEWC737 callsign.
  e.affiliation = "friend";         // BU: Ownship affiliation is not editable.
  e.platform = "aew";               // BU: Ownship platform is not editable.
  e.ownship = true;                 // BU: Keep the ownship flag set.
  if (e.alt_m <= 0) e.alt_m = 9500; // BU: If altitude was cleared, restore the default FL.
}

// BU: Serialize a waypoint to JSON {lat, lon}.
inline void to_json(nlohmann::json& j, const Waypoint& w) {
  j = nlohmann::json{{"lat", w.lat}, {"lon", w.lon}};
}

// BU: Deserialize a waypoint; both lat and lon are required.
inline void from_json(const nlohmann::json& j, Waypoint& w) {
  j.at("lat").get_to(w.lat);  // BU: Read required latitude.
  j.at("lon").get_to(w.lon);  // BU: Read required longitude.
}

// BU: Read affiliation, falling back to legacy "side" (blue/red/neutral).
inline std::string map_affiliation(const nlohmann::json& j) {
  if (j.contains("affiliation")) return j.at("affiliation").get<std::string>();  // BU: Prefer the modern field.
  if (j.contains("side")) {                                                      // BU: Older scenarios used blue/red.
    const auto s = j.at("side").get<std::string>();                              // BU: Read the legacy side string.
    if (s == "blue") return "friend";                                            // BU: Map blue force → friend.
    if (s == "red") return "hostile";                                            // BU: Map red force → hostile.
    if (s == "neutral") return "neutral";                                        // BU: Neutral stays neutral.
    return s;                                                                    // BU: Pass through any other side label.
  }
  return "friend";  // BU: Default identity if the JSON omitted both fields.
}

// BU: Read platform, falling back to legacy entity_type and mapping surface types to unknown.
inline std::string map_platform(const nlohmann::json& j) {
  std::string p;                                            // BU: Platform string we will normalize.
  if (j.contains("platform"))                               // BU: Modern scenarios store "platform".
    p = j.at("platform").get<std::string>();                // BU: Use the modern field.
  else if (j.contains("entity_type"))                       // BU: Older files used "entity_type".
    p = j.at("entity_type").get<std::string>();             // BU: Use the legacy field.
  else
    return "fighter";                                       // BU: Default air-picture type.
  // Legacy surface / generic → air-picture equivalents
  if (p == "aircraft") return "fighter";                    // BU: Generic aircraft becomes a fighter icon.
  if (p == "ship" || p == "ground" || p == "vehicle" || p == "missile")
    return "unknown";                                       // BU: Non-air leftovers become unknown air tracks.
  return p;                                                 // BU: Keep recognized air platforms as-is.
}

// BU: Serialize a full entity, including compatibility mirrors for old frontends.
inline void to_json(nlohmann::json& j, const Entity& e) {
  j = nlohmann::json{
      {"id", e.id},                      // BU: Track id.
      {"name", e.name},                  // BU: Callsign.
      {"affiliation", e.affiliation},    // BU: Identity.
      {"platform", e.platform},          // BU: Platform class.
      {"lat", e.lat},                    // BU: Latitude.
      {"lon", e.lon},                    // BU: Longitude.
      {"alt_m", e.alt_m},                // BU: Altitude meters MSL.
      {"heading_deg", e.heading_deg},    // BU: Heading.
      {"speed_mps", e.speed_mps},        // BU: Ground speed.
      {"climb_mps", e.climb_mps},        // BU: Vertical rate.
      {"iff_enabled", e.iff_enabled},    // BU: IFF master enable.
      {"iff_mode", e.iff_mode},          // BU: Active IFF mode.
      {"squawk", e.squawk},              // BU: Mode 3/A code.
      {"mode_c", e.mode_c},              // BU: Mode C flag.
      {"mode_4", e.mode_4},              // BU: Mode 4 flag.
      {"mode_5", e.mode_5},              // BU: Mode 5 flag.
      {"ownship", e.ownship},            // BU: Ownship flag.
      {"route", e.route},                // BU: Waypoint list.
      {"route_index", e.route_index},    // BU: Next waypoint index.
      // compatibility mirrors
      {"side", e.affiliation},           // BU: Legacy alias of affiliation.
      {"entity_type", e.platform},       // BU: Legacy alias of platform.
      {"alt", e.alt_m},                  // BU: Legacy alias of alt_m.
  };
}

// BU: Deserialize an entity from JSON, applying legacy field maps and ownship normalization.
inline void from_json(const nlohmann::json& j, Entity& e) {
  j.at("id").get_to(e.id);                                           // BU: Id is required.
  e.name = j.value("name", e.id);                                    // BU: Callsign defaults to id.
  e.affiliation = map_affiliation(j);                                // BU: Resolve affiliation / legacy side.
  e.platform = map_platform(j);                                      // BU: Resolve platform / legacy entity_type.
  j.at("lat").get_to(e.lat);                                         // BU: Latitude is required.
  j.at("lon").get_to(e.lon);                                         // BU: Longitude is required.
  if (j.contains("alt_m"))                                           // BU: Prefer modern altitude key.
    e.alt_m = j.at("alt_m").get<double>();                           // BU: Read alt_m.
  else
    e.alt_m = j.value("alt", 0.0);                                   // BU: Fall back to legacy "alt".
  e.heading_deg = j.value("heading_deg", 0.0);                       // BU: Heading, default north.
  e.speed_mps = j.value("speed_mps", 0.0);                           // BU: Speed, default static.
  e.climb_mps = j.value("climb_mps", 0.0);                           // BU: Climb, default level.
  e.iff_enabled = j.value("iff_enabled", true);                      // BU: IFF on by default.
  e.iff_mode = j.value("iff_mode", std::string{"3A"});               // BU: Default Mode 3/A.
  e.squawk = j.value("squawk", std::string{"1200"});                 // BU: Default VFR squawk.
  e.mode_c = j.value("mode_c", true);                                // BU: Mode C on by default.
  e.mode_4 = j.value("mode_4", false);                               // BU: Mode 4 off unless set.
  e.mode_5 = j.value("mode_5", false);                               // BU: Mode 5 off unless set.
  e.ownship = j.value("ownship", false);                             // BU: Ownship only if flagged.
  e.route = j.value("route", std::vector<Waypoint>{});               // BU: Route or empty list.
  e.route_index = j.value("route_index", static_cast<std::size_t>(0));  // BU: Start at first waypoint.
  if (is_ownship(e)) normalize_ownship(e);                           // BU: Lock ownship identity if this is AEWC737.
}

}  // namespace iwars
