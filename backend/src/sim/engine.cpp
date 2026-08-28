#include "iwars/sim/engine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace iwars {
namespace {

// BU: Mean Earth radius in meters for spherical dead-reckoning (not a full WGS84 ellipsoid).
constexpr double kEarthRadiusM = 6371000.0;
// BU: Pi used by deg/rad conversions and haversine.
constexpr double kPi = 3.14159265358979323846;

// BU: Convert decimal degrees to radians.
double deg2rad(double d) { return d * kPi / 180.0; }
// BU: Convert radians to decimal degrees.
double rad2deg(double r) { return r * 180.0 / kPi; }

// BU: Great-circle distance in meters between two lat/lon points (haversine).
double haversine_m(double lat1, double lon1, double lat2, double lon2) {
  const double dlat = deg2rad(lat2 - lat1);  // BU: Latitude delta in radians.
  const double dlon = deg2rad(lon2 - lon1);  // BU: Longitude delta in radians.
  const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +  // BU: Haversine 'a' term.
                   std::cos(deg2rad(lat1)) * std::cos(deg2rad(lat2)) *
                       std::sin(dlon / 2) * std::sin(dlon / 2);
  return 2 * kEarthRadiusM * std::asin(std::sqrt(a));  // BU: Central angle * Earth radius.
}

// BU: Initial true bearing (degrees, 0–360) from point 1 toward point 2.
double bearing_deg(double lat1, double lon1, double lat2, double lon2) {
  const double φ1 = deg2rad(lat1);           // BU: Start latitude in radians.
  const double φ2 = deg2rad(lat2);           // BU: Target latitude in radians.
  const double Δλ = deg2rad(lon2 - lon1);    // BU: Longitude delta in radians.
  const double y = std::sin(Δλ) * std::cos(φ2);  // BU: East component of the bearing.
  const double x = std::cos(φ1) * std::sin(φ2) -  // BU: North component of the bearing.
                   std::sin(φ1) * std::cos(φ2) * std::cos(Δλ);
  double b = rad2deg(std::atan2(y, x));      // BU: atan2 gives signed degrees from north.
  if (b < 0) b += 360.0;                     // BU: Wrap negative bearings into [0, 360).
  return b;                                  // BU: Heading the entity should fly to the waypoint.
}

}  // namespace

// BU: Store tick rate and derive the period used by sleep_until in loop().
Engine::Engine(double tick_hz)
    : tick_hz_(tick_hz), tick_period_(1.0 / tick_hz) {}

// BU: Ensure the worker is stopped before members (mutex, thread) are destroyed.
Engine::~Engine() { stop(); }

void Engine::start() {
  if (running_.exchange(true)) return;  // BU: Already running — do not spawn a second worker.
  {
    std::lock_guard lock(mu_);          // BU: Snapshot the live scenario as the reset baseline.
    initial_ = scenario_;               // BU: reset() will restore this copy.
  }
  worker_ = std::thread([this] { loop(); });  // BU: Launch the timed tick/publish loop.
}

void Engine::stop() {
  if (!running_.exchange(false)) return;  // BU: Already stopped — nothing to join.
  if (worker_.joinable()) worker_.join(); // BU: Wait until loop() observes running_==false and exits.
}

void Engine::play() {
  {
    std::lock_guard lock(mu_);          // BU: Flip the arm under the same lock the loop uses to snapshot.
    playing_.store(true);               // BU: Arm kinematics integration.
    bump_epoch();                       // BU: Invalidate any in-flight paused heartbeat.
  }
  emit_state();                         // BU: Push immediately so the UI does not wait up to one tick.
}

void Engine::pause() {
  {
    std::lock_guard lock(mu_);          // BU: Same lock as snapshot so playing and epoch stay paired.
    playing_.store(false);              // BU: Freeze integration; loop still heartbeats.
    bump_epoch();                       // BU: Force a paused push even if a playing tick already ran.
  }
  emit_state();                         // BU: Push the paused flag before the next 1 Hz idle slot.
}

void Engine::reset() {
  {
    std::lock_guard lock(mu_);                      // BU: Mutate scenario_ under the engine lock.
    scenario_.entities = initial_.entities;         // BU: Restore track list to the snapshot.
    scenario_.meta = initial_.meta;                 // BU: Restore camera/name.
    for (auto& e : scenario_.entities) e.route_index = 0;  // BU: Rewind every route to the first waypoint.
    scenario_.ensure_ownship();                     // BU: Re-inject AEWC737 if the snapshot was odd.
    sim_time_.store(0.0);                           // BU: Zero the simulation clock.
    bump_epoch();                                   // BU: Notify listeners that T+0 is live.
  }
  emit_state();                                     // BU: Push the restored picture immediately.
}

// BU: Relaxed add is enough: the loop only uses epoch inequality, not ordering.
void Engine::bump_epoch() { epoch_.fetch_add(1, std::memory_order_relaxed); }

std::uint64_t Engine::next_seq() const {
  return seq_.fetch_add(1, std::memory_order_relaxed) + 1;  // BU: First snapshot is 1, then 2, …
}

void Engine::emit_state() {
  StateCallback cb;                     // BU: Copy the callback so WS/UDP run without holding mu_.
  nlohmann::json state;                 // BU: Snapshot JSON.
  {
    std::lock_guard lock(mu_);          // BU: Pair scenario bytes with the live playing flag.
    state = scenario_.to_json();        // BU: Serialize live picture.
    state["playing"] = playing_.load(); // BU: Current arm, not a flag captured before this lock.
    state["sim_time"] = sim_time_.load();  // BU: Attach clock.
    state["seq"] = next_seq();          // BU: Monotonic id for the UI.
    last_pushed_epoch_.store(epoch_.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);  // BU: This epoch was actually sent.
    cb = on_state_;                     // BU: Copy std::function.
  }
  if (cb) cb(state);                    // BU: Broadcast + maybe UDP (see main.cpp).
}

void Engine::set_state_callback(StateCallback cb) {
  std::lock_guard lock(mu_);   // BU: Callback is copied under the lock in loop() too.
  on_state_ = std::move(cb);   // BU: Install the publisher (WS broadcast + UDP send).
}

nlohmann::json Engine::snapshot() const {
  std::lock_guard lock(mu_);            // BU: Lock so we do not race the tick thread.
  auto j = scenario_.to_json();         // BU: Serialize meta, udp, entities.
  j["playing"] = playing_.load();       // BU: Attach playback arm for the UI.
  j["sim_time"] = sim_time_.load();     // BU: Attach simulation clock for T+ display.
  j["seq"] = next_seq();                // BU: Same sequence space as WS pushes.
  return j;                             // BU: Return a detached JSON document.
}

Scenario Engine::copy_scenario() const {
  std::lock_guard lock(mu_);  // BU: Deep-copy under lock for UDP encode and HTTP save.
  return scenario_;           // BU: Return by value.
}

void Engine::replace_scenario(Scenario sc) {
  {
    std::lock_guard lock(mu_);     // BU: Swap the live picture atomically w.r.t. ticks.
    sc.ensure_ownship();           // BU: Guarantee AEWC737 exists before we go live.
    scenario_ = std::move(sc);     // BU: Become the new live scenario.
    initial_ = scenario_;          // BU: Reset baseline matches the newly loaded file.
    sim_time_.store(0.0);          // BU: New picture starts at T+0.
    bump_epoch();                  // BU: Mark the load so paused ticks cannot skip it.
  }
  emit_state();                    // BU: Push immediately so a paused editor sees the new file.
}

void Engine::set_udp(UdpConfig cfg) {
  std::lock_guard lock(mu_);        // BU: UDP block lives on the scenario.
  normalize_udp(cfg);               // BU: Force localhost; sync entity_port / legacy port.
  scenario_.udp = std::move(cfg);   // BU: Store ports + enable for snapshots and copy_scenario.
  bump_epoch();                     // BU: UI status pill (UDP ON/OFF) needs a push.
}

bool Engine::add_entity(Entity e) {
  std::lock_guard lock(mu_);                         // BU: Mutate the entity list under lock.
  if (is_ownship(e) || e.id == kOwnshipId) return false;  // BU: Clients cannot spawn a second ownship.
  if (scenario_.find(e.id)) return false;            // BU: Ids must be unique.
  scenario_.entities.push_back(std::move(e));        // BU: Append the new track.
  initial_.entities = scenario_.entities;            // BU: Treat add as editing the baseline (reset keeps it).
  bump_epoch();                                      // BU: Push so the map shows the new icon.
  return true;                                       // BU: Created.
}

bool Engine::update_entity(const Entity& e) {
  std::lock_guard lock(mu_);              // BU: Patch one track under lock.
  auto* cur = scenario_.find(e.id);       // BU: Locate the live entity.
  if (!cur) return false;                 // BU: Unknown id.
  Entity next = e;                        // BU: Start from the posted body.
  if (is_ownship(*cur) || is_ownship(next)) {
    normalize_ownship(next);              // BU: Lock id/callsign/platform; keep posted speed/pose/IFF/route.
  }
  *cur = next;                            // BU: Commit the patch to the live list.
  if (auto* init = initial_.find(e.id)) *init = next;  // BU: Keep reset() in sync with editor changes.
  bump_epoch();                           // BU: Push the updated track.
  return true;                            // BU: Updated.
}

bool Engine::remove_entity(const std::string& id) {
  std::lock_guard lock(mu_);              // BU: Delete under lock.
  if (id == kOwnshipId) return false;     // BU: Never delete ownship by id.
  if (const auto* e = scenario_.find(id); e && is_ownship(*e)) return false;  // BU: Also block by callsign/flag.
  auto& ents = scenario_.entities;        // BU: Live list.
  const auto it =                         // BU: Partition matching ids to the end (STL remove-erase).
      std::remove_if(ents.begin(), ents.end(),
                     [&](const Entity& e) { return e.id == id; });
  if (it == ents.end()) return false;     // BU: Id was not present.
  ents.erase(it, ents.end());             // BU: Actually drop the matching element(s).
  auto& init = initial_.entities;         // BU: Mirror delete on the reset snapshot.
  init.erase(std::remove_if(init.begin(), init.end(),
                            [&](const Entity& e) { return e.id == id; }),
             init.end());
  scenario_.ensure_ownship();             // BU: If something weird happened, put AEWC737 back.
  bump_epoch();                           // BU: Push so the icon disappears.
  return true;                            // BU: Deleted.
}

void Engine::loop() {
  using clock = std::chrono::steady_clock;  // BU: Monotonic clock so NTP steps do not stretch ticks.
  auto next = clock::now();                 // BU: Deadline of the current slot.
  int idle_ticks = 0;                       // BU: Counts paused ticks toward a 1 Hz heartbeat.

  while (running_.load()) {                 // BU: Exit when stop() clears the flag.
    next += std::chrono::duration_cast<clock::duration>(  // BU: Advance the slot by one tick period.
        std::chrono::duration<double>(tick_period_));

    const bool playing = playing_.load();   // BU: Snapshot the arm for this slot.
    if (playing) {                          // BU: Only integrate while playing.
      tick(tick_period_);                   // BU: Move every entity by dt.
      sim_time_.store(sim_time_.load() + tick_period_);  // BU: Advance the published clock.
    }

    // Playing: stream every tick. Paused: only on mutation or ~1 Hz heartbeat.
    const std::uint64_t ep = epoch_.load(std::memory_order_relaxed);  // BU: Current mutation counter.
    bool should_push = playing;             // BU: Playing always publishes (UI + UDP).
    if (!playing) {                         // BU: When paused, throttle to mutations + ~1 Hz.
      ++idle_ticks;                         // BU: Count this idle slot.
      if (ep != last_pushed_epoch_.load(std::memory_order_relaxed) ||
          idle_ticks >= static_cast<int>(tick_hz_)) {  // BU: Unsent mutation or ~1 s.
        should_push = true;                 // BU: Time to heartbeat the UI.
        idle_ticks = 0;                     // BU: Restart the idle counter.
      }
    } else {
      idle_ticks = 0;                       // BU: Playing ticks are not idle.
    }

    if (should_push) emit_state();          // BU: Serialize under lock so playing/epoch cannot tear.

    std::this_thread::sleep_until(next);    // BU: Sleep until the next 10 Hz slot (catches up if we ran long).
  }
}

void Engine::tick(double dt) {
  std::lock_guard lock(mu_);                // BU: Integrate under the same lock as HTTP mutations.
  for (auto& e : scenario_.entities) {      // BU: Advance every track, including ownship.
    advance_entity(e, dt);                  // BU: Route-follow or dead-reckon this entity.
  }
}

void Engine::advance_entity(Entity& e, double dt) {
  if (!e.route.empty()) {                                        // BU: A route is a closed loop — never drop off the end.
    if (e.route_index >= e.route.size()) e.route_index = 0;      // BU: Stale index from an older snapshot.
    const auto& wp = e.route[e.route_index];                     // BU: Current target waypoint.
    const double dist = haversine_m(e.lat, e.lon, wp.lat, wp.lon);  // BU: Meters remaining to the waypoint.
    e.heading_deg = bearing_deg(e.lat, e.lon, wp.lat, wp.lon);      // BU: Point the nose at the waypoint.
    if (dist < std::max(2.0, e.speed_mps * dt * 1.5)) {      // BU: Close enough this tick (2 m floor or 1.5*step).
      e.lat = wp.lat;                                        // BU: Snap onto the waypoint.
      e.lon = wp.lon;                                        // BU: Snap onto the waypoint.
      e.route_index = (e.route_index + 1) % e.route.size();  // BU: Next vertex, or wrap to WP1.
      return;                                                // BU: Do not also dead-reckon this same dt.
    }
  }

  if (e.speed_mps <= 0 && e.climb_mps == 0) return;  // BU: Fully static — skip integration.

  if (e.speed_mps > 0) {                             // BU: Horizontal motion along current heading.
    const double heading = deg2rad(e.heading_deg);   // BU: Heading in radians (0 = north).
    const double distance = e.speed_mps * dt;        // BU: Arc length this tick (meters).
    const double dlat = (distance * std::cos(heading)) / kEarthRadiusM;  // BU: Northing as radians of lat.
    const double dlon = (distance * std::sin(heading)) /                 // BU: Easting as radians of lon, scaled by cos(lat).
                        (kEarthRadiusM * std::cos(deg2rad(e.lat)));
    e.lat += rad2deg(dlat);                          // BU: Apply latitude delta.
    e.lon += rad2deg(dlon);                          // BU: Apply longitude delta.
  }
  e.alt_m += e.climb_mps * dt;                       // BU: Integrate vertical rate.
  if (e.alt_m < 0) e.alt_m = 0;                      // BU: Do not fly below ground.
}

}  // namespace iwars
