#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace iwars {

inline constexpr const char* kOwnshipId = "ownship";
inline constexpr const char* kOwnshipCallsign = "AEWC737";

struct Waypoint {
  double lat{0};
  double lon{0};
};

/** Truth target for radar/IFF stimulators.
 *  Downstream radar sim turns these into radar blips + IFF replies.
 */
struct Entity {
  std::string id;
  std::string name;  // callsign / track label

  // friend | assumed_friend | neutral | suspect | hostile | unknown
  std::string affiliation{"friend"};

  // Air C4ISR: fighter | attack | bomber | transport | tanker | aew | isr |
  // uav | ucav | helicopter | civil | unknown  (+ legacy aircraft)
  std::string platform{"fighter"};

  double lat{0};
  double lon{0};
  double alt_m{0};          // MSL meters (truth + Mode-C source)
  double heading_deg{0};    // 0 = north, 90 = east
  double speed_mps{0};
  double climb_mps{0};      // vertical rate

  // IFF / SIF (air C4ISR)
  bool iff_enabled{true};
  std::string iff_mode{"3A"};  // off | 1 | 2 | 3A | C | S | 4 | 5
  std::string squawk{"1200"};  // Mode 3/A octal code as string
  bool mode_c{true};           // report altitude via Mode C
  bool mode_4{false};          // crypto friend reply (stim flag)
  bool mode_5{false};          // Mode 5 stim flag
  bool ownship{false};         // fixed AEW ownship (AEWC737), always present / static

  std::vector<Waypoint> route;
  std::size_t route_index{0};

  // Legacy aliases kept in JSON for older scenarios
  // side -> affiliation, entity_type -> platform, alt -> alt_m
};

inline bool is_ownship(const Entity& e) {
  return e.ownship || e.id == kOwnshipId || e.name == kOwnshipCallsign;
}

/** Static B737 AEW&C ownship — present in every scenario, never moves. */
inline Entity make_ownship(double lat, double lon) {
  Entity e;
  e.id = kOwnshipId;
  e.name = kOwnshipCallsign;
  e.affiliation = "friend";
  e.platform = "aew";
  e.lat = lat;
  e.lon = lon;
  e.alt_m = 9500;
  e.heading_deg = 90;
  e.speed_mps = 0;
  e.climb_mps = 0;
  e.iff_enabled = true;
  e.iff_mode = "3A";
  e.squawk = "0001";
  e.mode_c = true;
  e.mode_4 = true;
  e.mode_5 = true;
  e.ownship = true;
  e.route.clear();
  e.route_index = 0;
  return e;
}

/** Force AEWC737 identity + static kinematics; keep pose / IFF edits. */
inline void normalize_ownship(Entity& e) {
  e.id = kOwnshipId;
  e.name = kOwnshipCallsign;
  e.affiliation = "friend";
  e.platform = "aew";
  e.ownship = true;
  e.speed_mps = 0;
  e.climb_mps = 0;
  e.route.clear();
  e.route_index = 0;
  if (e.alt_m <= 0) e.alt_m = 9500;
}

inline void to_json(nlohmann::json& j, const Waypoint& w) {
  j = nlohmann::json{{"lat", w.lat}, {"lon", w.lon}};
}

inline void from_json(const nlohmann::json& j, Waypoint& w) {
  j.at("lat").get_to(w.lat);
  j.at("lon").get_to(w.lon);
}

inline std::string map_affiliation(const nlohmann::json& j) {
  if (j.contains("affiliation")) return j.at("affiliation").get<std::string>();
  if (j.contains("side")) {
    const auto s = j.at("side").get<std::string>();
    if (s == "blue") return "friend";
    if (s == "red") return "hostile";
    if (s == "neutral") return "neutral";
    return s;
  }
  return "friend";
}

inline std::string map_platform(const nlohmann::json& j) {
  std::string p;
  if (j.contains("platform"))
    p = j.at("platform").get<std::string>();
  else if (j.contains("entity_type"))
    p = j.at("entity_type").get<std::string>();
  else
    return "fighter";
  // Legacy surface / generic → air-picture equivalents
  if (p == "aircraft") return "fighter";
  if (p == "ship" || p == "ground" || p == "vehicle" || p == "missile")
    return "unknown";
  return p;
}

inline void to_json(nlohmann::json& j, const Entity& e) {
  j = nlohmann::json{
      {"id", e.id},
      {"name", e.name},
      {"affiliation", e.affiliation},
      {"platform", e.platform},
      {"lat", e.lat},
      {"lon", e.lon},
      {"alt_m", e.alt_m},
      {"heading_deg", e.heading_deg},
      {"speed_mps", e.speed_mps},
      {"climb_mps", e.climb_mps},
      {"iff_enabled", e.iff_enabled},
      {"iff_mode", e.iff_mode},
      {"squawk", e.squawk},
      {"mode_c", e.mode_c},
      {"mode_4", e.mode_4},
      {"mode_5", e.mode_5},
      {"ownship", e.ownship},
      {"route", e.route},
      {"route_index", e.route_index},
      // compatibility mirrors
      {"side", e.affiliation},
      {"entity_type", e.platform},
      {"alt", e.alt_m},
  };
}

inline void from_json(const nlohmann::json& j, Entity& e) {
  j.at("id").get_to(e.id);
  e.name = j.value("name", e.id);
  e.affiliation = map_affiliation(j);
  e.platform = map_platform(j);
  j.at("lat").get_to(e.lat);
  j.at("lon").get_to(e.lon);
  if (j.contains("alt_m"))
    e.alt_m = j.at("alt_m").get<double>();
  else
    e.alt_m = j.value("alt", 0.0);
  e.heading_deg = j.value("heading_deg", 0.0);
  e.speed_mps = j.value("speed_mps", 0.0);
  e.climb_mps = j.value("climb_mps", 0.0);
  e.iff_enabled = j.value("iff_enabled", true);
  e.iff_mode = j.value("iff_mode", std::string{"3A"});
  e.squawk = j.value("squawk", std::string{"1200"});
  e.mode_c = j.value("mode_c", true);
  e.mode_4 = j.value("mode_4", false);
  e.mode_5 = j.value("mode_5", false);
  e.ownship = j.value("ownship", false);
  e.route = j.value("route", std::vector<Waypoint>{});
  e.route_index = j.value("route_index", static_cast<std::size_t>(0));
  if (is_ownship(e)) normalize_ownship(e);
}

}  // namespace iwars
