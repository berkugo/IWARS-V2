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
  double center_lat{39.9334};         // BU: Default map center latitude (Ankara).
  double center_lon{32.8597};         // BU: Default map center longitude (Ankara).
  int zoom{10};                       // BU: Default Leaflet zoom level.
};

// BU: Outbound UDP truth-feed destination and on/off switch.
struct UdpConfig {
  std::string host{"127.0.0.1"};  // BU: IPv4 address of the workplace radar/IFF listener.
  int port{9000};                 // BU: UDP destination port.
  bool enabled{false};            // BU: When false, the engine still ticks but sends no datagrams.
};

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

  /** Guarantee static AEWC737 ownship is present (first entity). */
  void ensure_ownship();                                   // BU: Deduplicate ownship-like tracks and insert AEWC737 first.
};

}  // namespace iwars
