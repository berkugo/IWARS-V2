// BU: Process entry: wire engine, HTTP, WebSocket, and UDP, then sleep until SIGINT/SIGTERM.
#include "iwars/api/server.hpp"
#include "iwars/net/udp_sender.hpp"
#include "iwars/sim/engine.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {

// BU: Process-wide run flag; signal handlers flip it so main can shut down cleanly.
std::atomic<bool> g_run{true};

// BU: SIGINT/SIGTERM handler — only stores false; no I/O in the handler.
void on_signal(int) { g_run.store(false); }

// BU: Find the scenarios/ folder from env, cwd, or paths relative to the executable.
std::filesystem::path resolve_scenarios_dir(int argc, char** argv) {
  if (const char* env = std::getenv("IWARS_SCENARIOS")) {  // BU: Explicit override wins.
    return env;                                            // BU: Use the env path as-is.
  }
  std::filesystem::path exe =                              // BU: Directory containing this binary.
      argc > 0 ? std::filesystem::absolute(argv[0]).parent_path()
               : std::filesystem::current_path();          // BU: Fall back to cwd if argv[0] is missing.
  // Try ../scenarios relative to build dir or cwd
  const std::filesystem::path candidates[] = {             // BU: Common layouts: repo root, next to exe, build/../..
      std::filesystem::current_path() / "scenarios",
      exe / "scenarios",
      exe / ".." / "scenarios",
      exe / ".." / ".." / "scenarios",
  };
  for (const auto& c : candidates) {                       // BU: Pick the first candidate that exists on disk.
    std::error_code ec;                                    // BU: Non-throwing exists() so a bad path does not abort.
    if (std::filesystem::exists(c, ec)) return std::filesystem::weakly_canonical(c);  // BU: Return a cleaned absolute path.
  }
  auto fallback = std::filesystem::current_path() / "scenarios";  // BU: Nothing found — use cwd/scenarios.
  std::filesystem::create_directories(fallback);           // BU: Create it so later saves have a place to write.
  return fallback;                                         // BU: Return the newly created (or existing) fallback.
}

// BU: Find the Vite production build (frontend/dist) so HTTP can serve the UI with no Node.
std::filesystem::path resolve_web_root(int argc, char** argv) {
  if (const char* env = std::getenv("IWARS_WEBROOT")) {    // BU: Explicit override wins.
    return env;
  }
  std::filesystem::path exe =
      argc > 0 ? std::filesystem::absolute(argv[0]).parent_path()
               : std::filesystem::current_path();
  const std::filesystem::path candidates[] = {
      std::filesystem::current_path() / "frontend" / "dist",
      exe / "frontend" / "dist",
      exe / ".." / "frontend" / "dist",
      exe / ".." / ".." / "frontend" / "dist",
      exe / "dist",
  };
  for (const auto& c : candidates) {
    std::error_code ec;
    if (std::filesystem::exists(c / "index.html", ec)) {
      return std::filesystem::weakly_canonical(c);
    }
  }
  return {};                                               // BU: Empty — API-only mode (Vite still works in dev).
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, on_signal);   // BU: Ctrl-C sets g_run false.
  std::signal(SIGTERM, on_signal);  // BU: systemd/kill also sets g_run false.

  const int http_port =                                    // BU: REST listen port from env or 8080.
      std::getenv("IWARS_HTTP_PORT") ? std::atoi(std::getenv("IWARS_HTTP_PORT"))
                                     : 8080;
  const int ws_port =                                      // BU: WebSocket listen port from env or 8081.
      std::getenv("IWARS_WS_PORT") ? std::atoi(std::getenv("IWARS_WS_PORT"))
                                   : 8081;

  const auto scenarios_dir = resolve_scenarios_dir(argc, argv);  // BU: Resolve where JSON scenarios live.
  const auto web_root = resolve_web_root(argc, argv);            // BU: Bundled UI, if present.
  std::cout << "[main] scenarios dir: " << scenarios_dir << "\n";  // BU: Log the chosen directory.
  if (!web_root.empty()) {
    std::cout << "[main] UI dir: " << web_root << "\n";
  } else {
    std::cout << "[main] UI dir: (none — open Vite :5173 or set IWARS_WEBROOT)\n";
  }

  iwars::Engine engine(10.0);  // BU: 10 Hz truth tick (100 ms period).
  iwars::UdpSender udp;        // BU: UDP sender with the placeholder IWP2 encoder.

  // Load demo scenario if present
  {
    const auto radar_demo = scenarios_dir / "demo_radar_air.json";  // BU: Preferred air-C4ISR demo.
    const auto legacy = scenarios_dir / "demo_ankara.json";         // BU: Older Ankara demo fallback.
    iwars::Scenario sc;                                             // BU: Temporary scenario we try to fill from disk.
    if (sc.load_file(radar_demo.string())) {                        // BU: Load the radar-air demo when it exists.
      engine.replace_scenario(sc);                                  // BU: Install it as the live + reset snapshot.
      udp.configure(sc.udp);                                        // BU: Apply the scenario's UDP host/port/enable.
      std::cout << "[main] loaded " << radar_demo << "\n";          // BU: Confirm which file won.
    } else if (sc.load_file(legacy.string())) {                     // BU: Else try the legacy Ankara file.
      engine.replace_scenario(sc);                                  // BU: Install the legacy picture.
      udp.configure(sc.udp);                                        // BU: Apply its UDP block.
      std::cout << "[main] loaded " << legacy << "\n";              // BU: Confirm legacy load.
    } else {                                                        // BU: No demo file — start empty over Ankara.
      iwars::Scenario empty;                                        // BU: Default-constructed scenario (Ankara camera).
      empty.meta.name = "blank";                                    // BU: Label it blank in the UI.
      empty.meta.center_lat = 39.9334;                              // BU: Ankara latitude.
      empty.meta.center_lon = 32.8597;                              // BU: Ankara longitude.
      empty.meta.zoom = 11;                                         // BU: Slightly closer than the struct default.
      engine.replace_scenario(empty);                               // BU: Ownship is injected inside replace_scenario.
    }
  }

  iwars::WsServer ws(ws_port);                                                 // BU: Live JSON feed for the React UI.
  iwars::HttpServer http(engine, udp, scenarios_dir.string(), web_root.string(),
                         http_port, ws_port);                                  // BU: REST + optional static UI.
  http.set_ws(&ws);                                                            // BU: Give HTTP a pointer to WS (engine callback does the actual broadcast).

  double sim_time = 0.0;                                                       // BU: Last sim_time seen while playing (used for UDP).
  engine.set_state_callback([&](const nlohmann::json& state) {                 // BU: Called every engine push (tick or heartbeat).
    ws.broadcast(state.dump());                                                // BU: Fan the JSON snapshot out to all WS clients (UI only; not DSS).
    if (state.value("playing", false)) {                                       // BU: UDP only while PLAYING. DSS heartbeat-while-paused → drop this guard.
      sim_time = state.value("sim_time", sim_time);                            // BU: Latch the clock carried in the snapshot.
      auto sc = engine.copy_scenario();                                        // BU: Copy entities + udp.enabled under the engine lock.
      if (sc.udp.enabled) {                                                    // BU: Respect the scenario/UI enable flag.
        udp.send(sc.entities, sim_time);                                       // BU: ICD: encoder_ packs Entity[] ; change send() if DSS wants one packet per track.
      }
    }
  });

  ws.start();      // BU: Bind WS and start accepting browsers.
  http.start();    // BU: Bind HTTP and start serving /api/*.
  engine.start();  // BU: Start the 10 Hz worker (paused until /control/play).

  std::cout << "[main] IWARS scenario tool ready\n"       // BU: Tell the operator where to connect.
            << "  UI    http://127.0.0.1:" << http_port << "/\n"
            << "  HTTP  http://127.0.0.1:" << http_port << "/api\n"
            << "  WS    ws://127.0.0.1:" << ws_port << "\n";

  while (g_run.load()) {                                   // BU: Idle the main thread until a stop signal.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));  // BU: 200 ms poll so shutdown is snappy.
  }

  std::cout << "[main] shutting down...\n";  // BU: Log orderly teardown.
  engine.stop();                             // BU: Join the tick thread first so no more callbacks fire.
  http.stop();                               // BU: Stop httplib listen + join.
  ws.stop();                                 // BU: Close WS clients and join accept.
  return 0;                                  // BU: Process exit success.
}
