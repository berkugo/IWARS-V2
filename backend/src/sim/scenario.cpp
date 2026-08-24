#include "iwars/sim/scenario.hpp"

#include <fstream>

namespace iwars {

nlohmann::json Scenario::to_json() const {
  return nlohmann::json{
      {"name", meta.name},
      {"description", meta.description},
      {"center_lat", meta.center_lat},
      {"center_lon", meta.center_lon},
      {"zoom", meta.zoom},
      {"udp",
       {{"host", udp.host}, {"port", udp.port}, {"enabled", udp.enabled}}},
      {"entities", entities},
  };
}

void Scenario::from_json(const nlohmann::json& j) {
  meta.name = j.value("name", std::string{"untitled"});
  meta.description = j.value("description", std::string{});
  meta.center_lat = j.value("center_lat", 39.9334);
  meta.center_lon = j.value("center_lon", 32.8597);
  meta.zoom = j.value("zoom", 10);
  if (j.contains("udp") && j["udp"].is_object()) {
    const auto& u = j["udp"];
    udp.host = u.value("host", std::string{"127.0.0.1"});
    udp.port = u.value("port", 9000);
    udp.enabled = u.value("enabled", false);
  }
  entities = j.value("entities", std::vector<Entity>{});
  ensure_ownship();
}

void Scenario::ensure_ownship() {
  // Drop duplicate ownship-like tracks; keep first match's pose if any
  Entity kept;
  bool found = false;
  std::vector<Entity> filtered;
  filtered.reserve(entities.size() + 1);
  for (auto& e : entities) {
    if (is_ownship(e)) {
      if (!found) {
        kept = e;
        found = true;
      }
      continue;
    }
    filtered.push_back(std::move(e));
  }
  if (found) {
    normalize_ownship(kept);
  } else {
    kept = make_ownship(meta.center_lat, meta.center_lon);
  }
  filtered.insert(filtered.begin(), std::move(kept));
  entities = std::move(filtered);
}

bool Scenario::load_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) return false;
  nlohmann::json j;
  in >> j;
  from_json(j);
  return true;
}

bool Scenario::save_file(const std::string& path) const {
  std::ofstream out(path);
  if (!out) return false;
  out << to_json().dump(2);
  return static_cast<bool>(out);
}

Entity* Scenario::find(const std::string& id) {
  for (auto& e : entities) {
    if (e.id == id) return &e;
  }
  return nullptr;
}

const Entity* Scenario::find(const std::string& id) const {
  for (const auto& e : entities) {
    if (e.id == id) return &e;
  }
  return nullptr;
}

}  // namespace iwars
