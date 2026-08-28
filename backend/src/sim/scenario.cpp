#include "iwars/sim/scenario.hpp"

#include <fstream>

namespace iwars {

nlohmann::json Scenario::to_json() const {
  return nlohmann::json{                                         // BU: Object the UI and files both understand.
      {"name", meta.name},                                       // BU: Scenario title.
      {"description", meta.description},                         // BU: Optional blurb.
      {"center_lat", meta.center_lat},                           // BU: Map camera latitude.
      {"center_lon", meta.center_lon},                           // BU: Map camera longitude.
      {"zoom", meta.zoom},                                       // BU: Leaflet zoom.
      {"udp", udp_to_json(udp)},
      {"entities", entities},                                    // BU: Array of Entity (uses to_json ADL).
  };
}

void Scenario::from_json(const nlohmann::json& j) {
  meta.name = j.value("name", std::string{"untitled"});          // BU: Name, or "untitled".
  meta.description = j.value("description", std::string{});      // BU: Description, or empty.
  meta.center_lat = j.value("center_lat", 38.9637);              // BU: Camera lat, default Turkey overview.
  meta.center_lon = j.value("center_lon", 35.2433);              // BU: Camera lon, default Turkey overview.
  meta.zoom = j.value("zoom", 6);                                // BU: Zoom default 6 (whole Turkey).
  if (j.contains("udp") && j["udp"].is_object()) {               // BU: Optional UDP object.
    udp = udp_from_json(j["udp"]);                               // BU: Localhost + entity/ownship ports.
  }
  entities = j.value("entities", std::vector<Entity>{});         // BU: Track list, or empty.
  ensure_ownship();                                              // BU: Always finish with AEWC737 at index 0.
}

void Scenario::ensure_ownship() {
  // Drop duplicate ownship-like tracks; keep first match's pose if any
  Entity kept;                                                   // BU: The one ownship we will keep.
  bool found = false;                                            // BU: Whether the file already had an ownship.
  std::vector<Entity> filtered;                                  // BU: Non-ownship tracks plus one ownship at front.
  filtered.reserve(entities.size() + 1);                         // BU: Avoid realloc while we scan.
  for (auto& e : entities) {                                     // BU: Walk every loaded track.
    if (is_ownship(e)) {                                         // BU: Id, callsign, or flag matches AEWC737.
      if (!found) {                                              // BU: Keep the first ownship's pose/IFF.
        kept = e;                                                // BU: Copy it aside.
        found = true;                                            // BU: Ignore later duplicates.
      }
      continue;                                                  // BU: Do not push ownship into the filtered body.
    }
    filtered.push_back(std::move(e));                            // BU: Preserve ordinary tracks in order.
  }
  if (found) {                                                   // BU: File had an ownship — lock identity, keep pose.
    normalize_ownship(kept);                                     // BU: Lock AEWC737 identity; keep pose/speed/route.
  } else {
    kept = make_ownship();                                       // BU: Synthesize AEWC737 at Ankara; camera may be Turkey-wide.
  }
  filtered.insert(filtered.begin(), std::move(kept));            // BU: Ownship is always the first entity.
  entities = std::move(filtered);                                // BU: Replace the list.
}

bool Scenario::load_file(const std::string& path) {
  std::ifstream in(path);                                        // BU: Open the JSON file.
  if (!in) return false;                                         // BU: Missing/unreadable file.
  nlohmann::json j;                                              // BU: Parse tree.
  in >> j;                                                       // BU: Stream-parse JSON (throws on syntax error).
  from_json(j);                                                  // BU: Fill meta/udp/entities + ensure ownship.
  return true;                                                   // BU: Loaded.
}

bool Scenario::save_file(const std::string& path) const {
  std::ofstream out(path);                                       // BU: Open/truncate the destination.
  if (!out) return false;                                        // BU: Cannot write (permissions/path).
  out << to_json().dump(2);                                      // BU: Pretty-print with 2-space indent.
  return static_cast<bool>(out);                                 // BU: False if the stream went bad mid-write.
}

Entity* Scenario::find(const std::string& id) {
  for (auto& e : entities) {                                     // BU: Linear scan is fine for small air pictures.
    if (e.id == id) return &e;                                   // BU: Mutable pointer into the vector.
  }
  return nullptr;                                                // BU: Not found.
}

const Entity* Scenario::find(const std::string& id) const {
  for (const auto& e : entities) {                               // BU: Const overload for snapshot/copy paths.
    if (e.id == id) return &e;                                   // BU: Const pointer into the vector.
  }
  return nullptr;                                                // BU: Not found.
}

}  // namespace iwars
