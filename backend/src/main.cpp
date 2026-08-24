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

std::atomic<bool> g_run{true};

void on_signal(int) { g_run.store(false); }

std::filesystem::path resolve_scenarios_dir(int argc, char** argv) {
  if (const char* env = std::getenv("IWARS_SCENARIOS")) {
    return env;
  }
  std::filesystem::path exe =
      argc > 0 ? std::filesystem::absolute(argv[0]).parent_path()
               : std::filesystem::current_path();
  // Try ../scenarios relative to build dir or cwd
  const std::filesystem::path candidates[] = {
      std::filesystem::current_path() / "scenarios",
      exe / "scenarios",
      exe / ".." / "scenarios",
      exe / ".." / ".." / "scenarios",
  };
  for (const auto& c : candidates) {
    std::error_code ec;
    if (std::filesystem::exists(c, ec)) return std::filesystem::weakly_canonical(c);
  }
  auto fallback = std::filesystem::current_path() / "scenarios";
  std::filesystem::create_directories(fallback);
  return fallback;
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  const int http_port =
      std::getenv("IWARS_HTTP_PORT") ? std::atoi(std::getenv("IWARS_HTTP_PORT"))
                                     : 8080;
  const int ws_port =
      std::getenv("IWARS_WS_PORT") ? std::atoi(std::getenv("IWARS_WS_PORT"))
                                   : 8081;

  const auto scenarios_dir = resolve_scenarios_dir(argc, argv);
  std::cout << "[main] scenarios dir: " << scenarios_dir << "\n";

  iwars::Engine engine(10.0);
  iwars::UdpSender udp;

  // Load demo scenario if present
  {
    const auto radar_demo = scenarios_dir / "demo_radar_air.json";
    const auto legacy = scenarios_dir / "demo_ankara.json";
    iwars::Scenario sc;
    if (sc.load_file(radar_demo.string())) {
      engine.replace_scenario(sc);
      udp.configure(sc.udp);
      std::cout << "[main] loaded " << radar_demo << "\n";
    } else if (sc.load_file(legacy.string())) {
      engine.replace_scenario(sc);
      udp.configure(sc.udp);
      std::cout << "[main] loaded " << legacy << "\n";
    } else {
      iwars::Scenario empty;
      empty.meta.name = "blank";
      empty.meta.center_lat = 39.9334;
      empty.meta.center_lon = 32.8597;
      empty.meta.zoom = 11;
      engine.replace_scenario(empty);
    }
  }

  iwars::WsServer ws(ws_port);
  iwars::HttpServer http(engine, udp, scenarios_dir.string(), http_port);
  http.set_ws(&ws);

  double sim_time = 0.0;
  engine.set_state_callback([&](const nlohmann::json& state) {
    ws.broadcast(state.dump());
    if (state.value("playing", false)) {
      sim_time = state.value("sim_time", sim_time);
      auto sc = engine.copy_scenario();
      if (sc.udp.enabled) {
        udp.send(sc.entities, sim_time);
      }
    }
  });

  ws.start();
  http.start();
  engine.start();

  std::cout << "[main] IWARS scenario tool ready\n"
            << "  HTTP  http://127.0.0.1:" << http_port << "\n"
            << "  WS    ws://127.0.0.1:" << ws_port << "\n";

  while (g_run.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  std::cout << "[main] shutting down...\n";
  engine.stop();
  http.stop();
  ws.stop();
  return 0;
}
