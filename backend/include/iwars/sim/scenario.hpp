#pragma once

#include "iwars/sim/entity.hpp"

#include <nlohmann/json.hpp>
#include <mutex>
#include <string>
#include <vector>

namespace iwars {

struct ScenarioMeta {
  std::string name{"untitled"};
  std::string description;
  double center_lat{39.9334};
  double center_lon{32.8597};
  int zoom{10};
};

struct UdpConfig {
  std::string host{"127.0.0.1"};
  int port{9000};
  bool enabled{false};
};

class Scenario {
 public:
  ScenarioMeta meta;
  UdpConfig udp;
  std::vector<Entity> entities;

  nlohmann::json to_json() const;
  void from_json(const nlohmann::json& j);
  bool load_file(const std::string& path);
  bool save_file(const std::string& path) const;

  Entity* find(const std::string& id);
  const Entity* find(const std::string& id) const;

  /** Guarantee static AEWC737 ownship is present (first entity). */
  void ensure_ownship();
};

}  // namespace iwars
