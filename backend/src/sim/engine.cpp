#include "iwars/sim/engine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace iwars {
namespace {

constexpr double kEarthRadiusM = 6371000.0;
constexpr double kPi = 3.14159265358979323846;

double deg2rad(double d) { return d * kPi / 180.0; }
double rad2deg(double r) { return r * 180.0 / kPi; }

double haversine_m(double lat1, double lon1, double lat2, double lon2) {
  const double dlat = deg2rad(lat2 - lat1);
  const double dlon = deg2rad(lon2 - lon1);
  const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(deg2rad(lat1)) * std::cos(deg2rad(lat2)) *
                       std::sin(dlon / 2) * std::sin(dlon / 2);
  return 2 * kEarthRadiusM * std::asin(std::sqrt(a));
}

double bearing_deg(double lat1, double lon1, double lat2, double lon2) {
  const double φ1 = deg2rad(lat1);
  const double φ2 = deg2rad(lat2);
  const double Δλ = deg2rad(lon2 - lon1);
  const double y = std::sin(Δλ) * std::cos(φ2);
  const double x = std::cos(φ1) * std::sin(φ2) -
                   std::sin(φ1) * std::cos(φ2) * std::cos(Δλ);
  double b = rad2deg(std::atan2(y, x));
  if (b < 0) b += 360.0;
  return b;
}

}  // namespace

Engine::Engine(double tick_hz)
    : tick_hz_(tick_hz), tick_period_(1.0 / tick_hz) {}

Engine::~Engine() { stop(); }

void Engine::start() {
  if (running_.exchange(true)) return;
  {
    std::lock_guard lock(mu_);
    initial_ = scenario_;
  }
  worker_ = std::thread([this] { loop(); });
}

void Engine::stop() {
  if (!running_.exchange(false)) return;
  if (worker_.joinable()) worker_.join();
}

void Engine::play() {
  playing_.store(true);
  bump_epoch();
}

void Engine::pause() {
  playing_.store(false);
  bump_epoch();
}

void Engine::reset() {
  std::lock_guard lock(mu_);
  scenario_.entities = initial_.entities;
  scenario_.meta = initial_.meta;
  for (auto& e : scenario_.entities) e.route_index = 0;
  scenario_.ensure_ownship();
  sim_time_.store(0.0);
  bump_epoch();
}

void Engine::bump_epoch() { epoch_.fetch_add(1, std::memory_order_relaxed); }

void Engine::set_state_callback(StateCallback cb) {
  std::lock_guard lock(mu_);
  on_state_ = std::move(cb);
}

nlohmann::json Engine::snapshot() const {
  std::lock_guard lock(mu_);
  auto j = scenario_.to_json();
  j["playing"] = playing_.load();
  j["sim_time"] = sim_time_.load();
  return j;
}

Scenario Engine::copy_scenario() const {
  std::lock_guard lock(mu_);
  return scenario_;
}

void Engine::replace_scenario(Scenario sc) {
  std::lock_guard lock(mu_);
  sc.ensure_ownship();
  scenario_ = std::move(sc);
  initial_ = scenario_;
  sim_time_.store(0.0);
  bump_epoch();
}

void Engine::set_udp(UdpConfig cfg) {
  std::lock_guard lock(mu_);
  scenario_.udp = std::move(cfg);
  bump_epoch();
}

bool Engine::add_entity(Entity e) {
  std::lock_guard lock(mu_);
  if (is_ownship(e) || e.id == kOwnshipId) return false;
  if (scenario_.find(e.id)) return false;
  scenario_.entities.push_back(std::move(e));
  initial_.entities = scenario_.entities;
  bump_epoch();
  return true;
}

bool Engine::update_entity(const Entity& e) {
  std::lock_guard lock(mu_);
  auto* cur = scenario_.find(e.id);
  if (!cur) return false;
  Entity next = e;
  if (is_ownship(*cur) || is_ownship(next)) {
    next.lat = e.lat;
    next.lon = e.lon;
    next.alt_m = e.alt_m;
    next.heading_deg = e.heading_deg;
    next.iff_enabled = e.iff_enabled;
    next.iff_mode = e.iff_mode;
    next.squawk = e.squawk;
    next.mode_c = e.mode_c;
    next.mode_4 = e.mode_4;
    next.mode_5 = e.mode_5;
    normalize_ownship(next);
  }
  *cur = next;
  if (auto* init = initial_.find(e.id)) *init = next;
  bump_epoch();
  return true;
}

bool Engine::remove_entity(const std::string& id) {
  std::lock_guard lock(mu_);
  if (id == kOwnshipId) return false;
  if (const auto* e = scenario_.find(id); e && is_ownship(*e)) return false;
  auto& ents = scenario_.entities;
  const auto it =
      std::remove_if(ents.begin(), ents.end(),
                     [&](const Entity& e) { return e.id == id; });
  if (it == ents.end()) return false;
  ents.erase(it, ents.end());
  auto& init = initial_.entities;
  init.erase(std::remove_if(init.begin(), init.end(),
                            [&](const Entity& e) { return e.id == id; }),
             init.end());
  scenario_.ensure_ownship();
  bump_epoch();
  return true;
}

void Engine::loop() {
  using clock = std::chrono::steady_clock;
  auto next = clock::now();
  std::uint64_t last_epoch = 0;
  int idle_ticks = 0;

  while (running_.load()) {
    next += std::chrono::duration_cast<clock::duration>(
        std::chrono::duration<double>(tick_period_));

    const bool playing = playing_.load();
    if (playing) {
      tick(tick_period_);
      sim_time_.store(sim_time_.load() + tick_period_);
    }

    // Playing: stream every tick. Paused: only on mutation or ~1 Hz heartbeat.
    const std::uint64_t ep = epoch_.load(std::memory_order_relaxed);
    bool should_push = playing;
    if (!playing) {
      ++idle_ticks;
      if (ep != last_epoch || idle_ticks >= static_cast<int>(tick_hz_)) {
        should_push = true;
        idle_ticks = 0;
      }
    }

    if (should_push) {
      last_epoch = ep;
      StateCallback cb;
      nlohmann::json state;
      {
        std::lock_guard lock(mu_);
        state = scenario_.to_json();
        state["playing"] = playing;
        state["sim_time"] = sim_time_.load();
        cb = on_state_;
      }
      if (cb) cb(state);
    }

    std::this_thread::sleep_until(next);
  }
}

void Engine::tick(double dt) {
  std::lock_guard lock(mu_);
  for (auto& e : scenario_.entities) {
    advance_entity(e, dt);
  }
}

void Engine::advance_entity(Entity& e, double dt) {
  if (is_ownship(e)) return;  // AEWC737 ownship is always static

  if (!e.route.empty() && e.route_index < e.route.size()) {
    const auto& wp = e.route[e.route_index];
    const double dist = haversine_m(e.lat, e.lon, wp.lat, wp.lon);
    e.heading_deg = bearing_deg(e.lat, e.lon, wp.lat, wp.lon);
    if (dist < std::max(2.0, e.speed_mps * dt * 1.5)) {
      e.lat = wp.lat;
      e.lon = wp.lon;
      ++e.route_index;
      if (e.route_index >= e.route.size()) {
        // Route finished — stop once; leave route_index at size so we
        // do not re-enter and force speed=0 every tick.
        e.speed_mps = 0;
      }
      return;
    }
  }

  if (e.speed_mps <= 0 && e.climb_mps == 0) return;

  if (e.speed_mps > 0) {
    const double heading = deg2rad(e.heading_deg);
    const double distance = e.speed_mps * dt;
    const double dlat = (distance * std::cos(heading)) / kEarthRadiusM;
    const double dlon = (distance * std::sin(heading)) /
                        (kEarthRadiusM * std::cos(deg2rad(e.lat)));
    e.lat += rad2deg(dlat);
    e.lon += rad2deg(dlon);
  }
  e.alt_m += e.climb_mps * dt;
  if (e.alt_m < 0) e.alt_m = 0;
}

}  // namespace iwars
