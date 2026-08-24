#pragma once

#include "iwars/sim/scenario.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace iwars {

// BU: Real-time truth engine: ticks entities, snapshots state, and notifies listeners.
class Engine {
 public:
  using StateCallback = std::function<void(const nlohmann::json&)>;  // BU: Called with a JSON snapshot to push over WS/UDP.

  explicit Engine(double tick_hz = 10.0);  // BU: Construct with a tick rate (default 10 Hz).
  ~Engine();                               // BU: Stop the worker thread if it is still running.

  Engine(const Engine&) = delete;             // BU: Not copyable — owns a thread and mutex-guarded state.
  Engine& operator=(const Engine&) = delete;  // BU: Not assignable for the same reason.

  void start();  // BU: Spawn the tick thread and snapshot the initial scenario for reset().
  void stop();   // BU: Request the loop to exit and join the worker.

  void play();   // BU: Arm integration so entities start moving.
  void pause();  // BU: Freeze integration; UI still receives heartbeats.
  void reset();  // BU: Restore entities/meta from the snapshot taken at start/replace.

  bool running() const { return running_.load(); }     // BU: True while the worker thread is alive.
  bool playing() const { return playing_.load(); }     // BU: True while kinematics are being integrated.
  double sim_time() const { return sim_time_.load(); } // BU: Elapsed simulation seconds since last reset.

  nlohmann::json snapshot() const;          // BU: Locked copy of scenario JSON plus playing/sim_time.
  Scenario copy_scenario() const;           // BU: Locked deep copy of the live scenario.
  void replace_scenario(Scenario sc);       // BU: Swap in a new scenario, ensure ownship, reset time.
  void set_udp(UdpConfig cfg);              // BU: Update the scenario's UDP block without replacing entities.

  bool add_entity(Entity e);                // BU: Insert a non-ownship track; false on duplicate/ownship id.
  bool update_entity(const Entity& e);      // BU: Patch a track by id; ownship kinematics stay static.
  bool remove_entity(const std::string& id);  // BU: Delete a track; ownship cannot be removed.

  void set_state_callback(StateCallback cb);  // BU: Install the WS/UDP publisher invoked each push.

 private:
  void loop();                          // BU: Timed 10 Hz loop: tick, maybe push state, sleep until next slot.
  void tick(double dt);                 // BU: Integrate every entity by dt seconds under the mutex.
  void advance_entity(Entity& e, double dt);  // BU: Steer along route or dead-reckon heading/speed/climb.
  void bump_epoch();                    // BU: Increment the mutation counter so a paused loop still pushes.

  double tick_hz_;                      // BU: Configured ticks per second.
  double tick_period_;                  // BU: Seconds per tick (1 / tick_hz_).
  std::atomic<bool> running_{false};    // BU: Worker-thread lifetime flag.
  std::atomic<bool> playing_{false};    // BU: Integration arm.
  std::atomic<double> sim_time_{0.0};   // BU: Simulation clock in seconds.
  std::atomic<std::uint64_t> epoch_{1}; // BU: Bumped on any mutation so paused UI still refreshes.
  mutable std::mutex mu_;               // BU: Guards scenario_, initial_, and on_state_.
  Scenario scenario_;                   // BU: Live picture the tick loop mutates.
  Scenario initial_;                    // BU: Snapshot used by reset().
  StateCallback on_state_;              // BU: Optional publisher (broadcast + UDP).
  std::thread worker_;                  // BU: Background loop() thread.
};

}  // namespace iwars
