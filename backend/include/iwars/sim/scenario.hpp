#pragma once

#include "iwars/sim/entity.hpp"

#include <nlohmann/json.hpp>
#include <mutex>
#include <string>
#include <vector>

namespace iwars {

// BU: Map/UI metadata stored with a scenario (name, description, camera).
struct ScenarioMeta {
  std::string name{"untitled"};       // BU: Scenario display name and default save stem.
  std::string description;            // BU: Free-text description shown in the editor.
  double center_lat{38.9637};         // BU: Default map center — geographic middle of Turkey.
  double center_lon{35.2433};         // BU: Default map center longitude (Turkey).
  int zoom{6};                        // BU: Default Leaflet zoom — whole-country view.
};

// BU: Outbound UDP truth feeds (always 127.0.0.1 for now — local DSS / stim).
struct UdpConfig {
  std::string host{"127.0.0.1"};  // BU: Forced localhost; UI does not let the operator pick an IP yet.
  int entity_port{9000};          // BU: Entity-truth datagrams (all tracks except ownship).
  int ownship_port{9001};         // BU: Ownship-truth datagrams (AEWC737 only).
  int port{9000};                 // BU: Legacy alias of entity_port (older scenario JSON).
  bool enabled{false};            // BU: When false, the engine still ticks but sends no datagrams.
};

// BU: Always local; keep entity_port in sync with legacy "port".
inline void normalize_udp(UdpConfig& c) {
  c.host = "127.0.0.1";
  if (c.entity_port <= 0) c.entity_port = c.port > 0 ? c.port : 9000;
  if (c.ownship_port <= 0) c.ownship_port = 9001;
  c.port = c.entity_port;
}

inline nlohmann::json udp_to_json(const UdpConfig& c) {
  UdpConfig n = c;
  normalize_udp(n);
  return nlohmann::json{
      {"host", n.host},
      {"port", n.entity_port},
      {"entity_port", n.entity_port},
      {"ownship_port", n.ownship_port},
      {"enabled", n.enabled},
  };
}

inline UdpConfig udp_from_json(const nlohmann::json& u) {
  UdpConfig c;
  c.entity_port = u.value("entity_port", u.value("port", 9000));
  c.ownship_port = u.value("ownship_port", 9001);
  c.enabled = u.value("enabled", false);
  normalize_udp(c);
  return c;
}

// BU: In-memory scenario: camera meta, UDP config, and the entity list.
class Scenario {
 public:
  ScenarioMeta meta;               // BU: Name / description / map camera.
  UdpConfig udp;                   // BU: UDP destination baked into the scenario file.
  std::vector<Entity> entities;    // BU: All tracks; ownship is kept at index 0.

  nlohmann::json to_json() const;                          // BU: Serialize the whole scenario to JSON.
  void from_json(const nlohmann::json& j);                 // BU: Load fields from JSON and ensure ownship.
  bool load_file(const std::string& path);                 // BU: Read a .json file from disk; false on I/O error.
  bool save_file(const std::string& path) const;           // BU: Pretty-print JSON to disk; false on I/O error.

  Entity* find(const std::string& id);                     // BU: Mutable lookup by track id, or nullptr.
  const Entity* find(const std::string& id) const;         // BU: Const lookup by track id, or nullptr.

  /** Guarantee AEWC737 ownship is present (first entity). */
  void ensure_ownship();                                   // BU: Deduplicate ownship-like tracks and insert AEWC737 first.
};

}  // namespace iwars
