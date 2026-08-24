#pragma once

#include "iwars/sim/scenario.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace iwars {

class Engine {
 public:
  using StateCallback = std::function<void(const nlohmann::json&)>;

  explicit Engine(double tick_hz = 10.0);
  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  void start();
  void stop();

  void play();
  void pause();
  void reset();

  bool running() const { return running_.load(); }
  bool playing() const { return playing_.load(); }
  double sim_time() const { return sim_time_.load(); }

  nlohmann::json snapshot() const;
  Scenario copy_scenario() const;
  void replace_scenario(Scenario sc);
  void set_udp(UdpConfig cfg);

  bool add_entity(Entity e);
  bool update_entity(const Entity& e);
  bool remove_entity(const std::string& id);

  void set_state_callback(StateCallback cb);

 private:
  void loop();
  void tick(double dt);
  void advance_entity(Entity& e, double dt);
  void bump_epoch();

  double tick_hz_;
  double tick_period_;
  std::atomic<bool> running_{false};
  std::atomic<bool> playing_{false};
  std::atomic<double> sim_time_{0.0};
  std::atomic<std::uint64_t> epoch_{1};
  mutable std::mutex mu_;
  Scenario scenario_;
  Scenario initial_;
  StateCallback on_state_;
  std::thread worker_;
};

}  // namespace iwars
