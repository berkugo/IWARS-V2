#include "iwars/api/server.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <mutex>

namespace fs = std::filesystem;  // BU: Short alias for scenario directory iteration.

namespace iwars {

namespace {
// BU: Small helper so PUT /api/udp can echo the stored config.
nlohmann::json udp_to_json(const UdpConfig& c) {
  return nlohmann::json{
      {"host", c.host}, {"port", c.port}, {"enabled", c.enabled}};
}

std::mutex g_http_mu;                 // BU: Guards g_http_svr so stop() can call Server::stop from another thread.
httplib::Server* g_http_svr = nullptr;  // BU: Pointer to the live httplib instance, or null when not listening.
}  // namespace

HttpServer::HttpServer(Engine& engine, UdpSender& udp, std::string scenarios_dir,
                       std::string web_root, int port, int ws_port)
    : engine_(engine),                         // BU: Engine this API mutates.
      udp_(udp),                               // BU: UDP sender reconfigured with scenario/UI settings.
      scenarios_dir_(std::move(scenarios_dir)),  // BU: Folder of *.json scenarios.
      web_root_(std::move(web_root)),          // BU: Bundled React dist, or empty.
      port_(port),                             // BU: Listen port.
      ws_port_(ws_port) {}                     // BU: Published in /api/health for the bundled UI.

HttpServer::~HttpServer() { stop(); }  // BU: Join the listen thread on destruction.

void HttpServer::set_ws(WsServer* ws) { ws_ = ws; }  // BU: Store WS pointer (broadcasts are driven from Engine callback).

void HttpServer::stop() {
  {
    std::lock_guard lock(g_http_mu);   // BU: httplib::Server::stop is thread-safe if we still have the pointer.
    if (g_http_svr) g_http_svr->stop();  // BU: Unblock listen() so the worker thread can exit.
  }
  if (thread_.joinable()) thread_.join();  // BU: Wait until the listen thread has cleared g_http_svr.
  running_.store(false);                   // BU: Mark stopped even if start() never ran.
}

void HttpServer::start() {
  if (running_.exchange(true)) return;  // BU: Already listening — ignore a second start.
  thread_ = std::thread([this] {        // BU: All routes live on this thread's stack-bound Server.
    httplib::Server svr;                // BU: cpp-httplib instance; listen() blocks until stop().
    {
      std::lock_guard lock(g_http_mu);  // BU: Publish the pointer for stop().
      g_http_svr = &svr;                // BU: stop() will call svr.stop() through this.
    }

    auto cors = [](const httplib::Request&, httplib::Response& res) {  // BU: Allow the Vite origin to call /api/*.
      res.set_header("Access-Control-Allow-Origin", "*");              // BU: Any origin (dev UI + tools).
      res.set_header("Access-Control-Allow-Headers", "Content-Type");  // BU: JSON bodies.
      res.set_header("Access-Control-Allow-Methods",                   // BU: Methods the frontend uses.
                     "GET, POST, PUT, DELETE, OPTIONS");
    };

    svr.Options(R"(/.*)",                                              // BU: CORS preflight for any path.
                [&](const httplib::Request& req, httplib::Response& res) {
                  cors(req, res);                                      // BU: Attach CORS headers.
                  res.status = 204;                                    // BU: No body.
                });

    svr.Get("/api/health",                                             // BU: Liveness probe.
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);                                          // BU: CORS on every JSON route.
              res.set_content(
                  nlohmann::json{{"ok", true}, {"ws_port", ws_port_}}.dump(),
                  "application/json");   // BU: UI uses ws_port when not behind the Vite proxy.
            });

    svr.Get("/api/state",                                              // BU: One-shot snapshot (WS is the live path).
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);                                          // BU: CORS.
              res.set_content(engine_.snapshot().dump(), "application/json");  // BU: Full scenario + playing + sim_time.
            });

    svr.Post("/api/control/play",                                      // BU: Arm the engine.
             [&](const httplib::Request& req, httplib::Response& res) {
               cors(req, res);                                         // BU: CORS.
               engine_.play();                                         // BU: Start integrating kinematics.
               res.set_content(engine_.snapshot().dump(), "application/json");  // BU: Full picture so the UI does not wait on WS.
             });

    svr.Post("/api/control/pause",                                     // BU: Freeze kinematics.
             [&](const httplib::Request& req, httplib::Response& res) {
               cors(req, res);                                         // BU: CORS.
               engine_.pause();                                        // BU: Stop integrating; clock holds.
               res.set_content(engine_.snapshot().dump(), "application/json");  // BU: Full picture so the UI does not wait on WS.
             });

    svr.Post("/api/control/reset",                                     // BU: Restore baseline entities and T=0.
             [&](const httplib::Request& req, httplib::Response& res) {
               cors(req, res);                                         // BU: CORS.
               engine_.pause();                                        // BU: Pause first so reset does not keep flying.
               engine_.reset();                                        // BU: Restore initial_ snapshot.
               res.set_content(engine_.snapshot().dump(), "application/json");  // BU: Return the restored picture.
             });

    svr.Put("/api/udp",                                                // BU: Update outbound UDP destination / enable.
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);                                          // BU: CORS.
              try {
                auto j = nlohmann::json::parse(req.body);              // BU: Parse {host,port,enabled}.
                UdpConfig cfg;                                         // BU: Fill with defaults then overlay JSON.
                cfg.host = j.value("host", std::string{"127.0.0.1"});  // BU: Destination IPv4.
                cfg.port = j.value("port", 9000);                      // BU: Destination port.
                cfg.enabled = j.value("enabled", false);               // BU: Send enable.
                engine_.set_udp(cfg);                                  // BU: Store on the live scenario (snapshots include it).
                udp_.configure(cfg);                                   // BU: Recreate the socket toward the new dest.
                res.set_content(udp_to_json(cfg).dump(), "application/json");  // BU: Echo the applied config.
              } catch (const std::exception& e) {                      // BU: Bad JSON or type error.
                res.status = 400;                                      // BU: Client error.
                res.set_content(nlohmann::json{{"error", e.what()}}.dump(),
                                "application/json");                   // BU: Error string for the UI.
              }
            });

    svr.Post("/api/entities",                                          // BU: Create a track.
             [&](const httplib::Request& req, httplib::Response& res) {
               cors(req, res);                                         // BU: CORS.
               try {
                 Entity e = nlohmann::json::parse(req.body).get<Entity>();  // BU: Deserialize via from_json.
                 if (e.id.empty()) {                                   // BU: Id is required.
                   res.status = 400;                                   // BU: Bad request.
                   res.set_content(R"({"error":"id required"})",
                                   "application/json");
                   return;                                             // BU: Do not call add_entity.
                 }
                 if (!engine_.add_entity(std::move(e))) {              // BU: Reject duplicate id or ownship spawn.
                   res.status = 409;                                   // BU: Conflict.
                   res.set_content(R"({"error":"id exists"})",
                                   "application/json");
                   return;                                             // BU: Nothing created.
                 }
                 res.set_content(engine_.snapshot().dump(), "application/json");  // BU: Return the new picture.
               } catch (const std::exception& ex) {                    // BU: Parse/schema error.
                 res.status = 400;                                     // BU: Bad request.
                 res.set_content(nlohmann::json{{"error", ex.what()}}.dump(),
                                 "application/json");
               }
             });

    svr.Put(R"(/api/entities/([^/]+))",                                // BU: Update track by id in the URL.
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);                                          // BU: CORS.
              try {
                Entity e = nlohmann::json::parse(req.body).get<Entity>();  // BU: Posted fields.
                e.id = req.matches[1];                                 // BU: URL id wins over body id.
                if (!engine_.update_entity(e)) {                       // BU: 404 if the id is unknown.
                  res.status = 404;                                    // BU: Not found.
                  res.set_content(R"({"error":"not found"})",
                                  "application/json");
                  return;                                              // BU: No snapshot.
                }
                res.set_content(engine_.snapshot().dump(), "application/json");  // BU: Return updated picture.
              } catch (const std::exception& ex) {                     // BU: Parse error.
                res.status = 400;                                      // BU: Bad request.
                res.set_content(nlohmann::json{{"error", ex.what()}}.dump(),
                                "application/json");
              }
            });

    svr.Delete(R"(/api/entities/([^/]+))",                             // BU: Delete a track; ownship is forbidden.
               [&](const httplib::Request& req, httplib::Response& res) {
                 cors(req, res);                                       // BU: CORS.
                 if (!engine_.remove_entity(req.matches[1])) {         // BU: False for missing id or ownship.
                   const std::string id = req.matches[1];              // BU: Capture id for the error body.
                   res.status = (id == iwars::kOwnshipId) ? 403 : 404;  // BU: 403 ownship, 404 unknown.
                   res.set_content(
                       id == iwars::kOwnshipId
                           ? R"({"error":"ownship AEWC737 cannot be removed"})"
                           : R"({"error":"not found"})",
                       "application/json");
                   return;                                             // BU: List unchanged.
                 }
                 res.set_content(engine_.snapshot().dump(), "application/json");  // BU: Return picture without the track.
               });

    svr.Get("/api/scenarios",                                          // BU: List *.json names in scenarios_dir_.
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);                                          // BU: CORS.
              nlohmann::json list = nlohmann::json::array();           // BU: Filename array.
              std::error_code ec;                                      // BU: Non-throwing exists().
              if (fs::exists(scenarios_dir_, ec)) {                    // BU: Skip if the folder is missing.
                for (const auto& entry :
                     fs::directory_iterator(scenarios_dir_)) {         // BU: One entry per file/dir.
                  if (entry.path().extension() == ".json") {           // BU: Only scenario files.
                    list.push_back(entry.path().filename().string());  // BU: Basename only (no path traversal).
                  }
                }
              }
              res.set_content(list.dump(), "application/json");        // BU: e.g. ["demo_radar_air.json"].
            });

    svr.Get(R"(/api/scenarios/([^/]+))",                               // BU: Load a file into the engine.
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);                                          // BU: CORS.
              const std::string name = req.matches[1];                 // BU: Filename from the URL.
              const fs::path path = fs::path(scenarios_dir_) / name;   // BU: Resolve under the scenarios folder.
              Scenario sc;                                             // BU: Parse target.
              if (!sc.load_file(path.string())) {                      // BU: Missing or unreadable.
                res.status = 404;                                      // BU: Not found.
                res.set_content(R"({"error":"not found"})", "application/json");
                return;                                                // BU: Engine unchanged.
              }
              engine_.replace_scenario(sc);                            // BU: Swap live + reset snapshot, T=0.
              udp_.configure(sc.udp);                                  // BU: Point UDP at the file's destination.
              res.set_content(engine_.snapshot().dump(), "application/json");  // BU: Return the loaded picture.
            });

    svr.Post("/api/scenarios",                                         // BU: Save current (or posted) scenario to disk.
             [&](const httplib::Request& req, httplib::Response& res) {
               cors(req, res);                                         // BU: CORS.
               try {
                 auto j = nlohmann::json::parse(req.body);             // BU: {filename,name,scenario?}.
                 std::string filename = j.value("filename", std::string{});  // BU: Preferred file name.
                 if (filename.empty()) {                               // BU: Derive from the scenario name.
                   filename = j.value("name", "untitled") + ".json";   // BU: untitled.json by default.
                 }
                 if (filename.find("..") != std::string::npos) {       // BU: Block path traversal.
                   res.status = 400;                                   // BU: Bad filename.
                   res.set_content(R"({"error":"bad filename"})",
                                   "application/json");
                   return;                                             // BU: Do not write.
                 }
                 Scenario sc = engine_.copy_scenario();                // BU: Start from the live picture.
                 if (j.contains("scenario")) {                         // BU: Optional full document to persist instead.
                   sc.from_json(j["scenario"]);                        // BU: Overlay posted scenario.
                   engine_.replace_scenario(sc);                       // BU: Also become live.
                   udp_.configure(sc.udp);                             // BU: Apply its UDP block.
                 }
                 fs::create_directories(scenarios_dir_);               // BU: Ensure the folder exists.
                 const fs::path path = fs::path(scenarios_dir_) / filename;  // BU: Destination path.
                 if (!sc.save_file(path.string())) {                   // BU: Pretty-print JSON.
                   res.status = 500;                                   // BU: Disk error.
                   res.set_content(R"({"error":"write failed"})",
                                   "application/json");
                   return;                                             // BU: Tell the UI it did not persist.
                 }
                 res.set_content(
                     nlohmann::json{{"filename", filename}, {"ok", true}}.dump(),
                     "application/json");                              // BU: Confirm the written name.
               } catch (const std::exception& ex) {                    // BU: Parse error.
                 res.status = 400;                                     // BU: Bad request.
                 res.set_content(nlohmann::json{{"error", ex.what()}}.dump(),
                                 "application/json");
               }
             });

    svr.Put("/api/scenario",                                           // BU: Replace in-memory scenario without writing a file.
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);                                          // BU: CORS.
              try {
                Scenario sc;                                           // BU: Empty then fill from body.
                sc.from_json(nlohmann::json::parse(req.body));         // BU: Parse the full document.
                engine_.replace_scenario(sc);                          // BU: Become live + reset baseline.
                udp_.configure(sc.udp);                                // BU: Apply UDP from the document.
                res.set_content(engine_.snapshot().dump(), "application/json");  // BU: Echo the installed picture.
              } catch (const std::exception& ex) {                     // BU: Parse error.
                res.status = 400;                                      // BU: Bad request.
                res.set_content(nlohmann::json{{"error", ex.what()}}.dump(),
                                "application/json");
              }
            });

    if (!web_root_.empty()) {                                          // BU: Air-gap UI: serve Vite dist from this process.
      std::error_code ec;
      if (fs::is_directory(web_root_, ec)) {
        if (svr.set_mount_point("/", web_root_)) {
          std::cout << "[http] UI dir: " << web_root_ << "\n";         // BU: Browser can open http://host:port/ with no Node.
        } else {
          std::cerr << "[http] failed to mount UI " << web_root_ << "\n";
        }
      } else {
        std::cerr << "[http] UI dir missing: " << web_root_ << "\n";
      }
    }

    std::cout << "[http] listening on 0.0.0.0:" << port_ << "\n";  // BU: Log bind address.
    svr.listen("0.0.0.0", port_);                                  // BU: Block until stop() — all interfaces.
    {
      std::lock_guard lock(g_http_mu);                             // BU: Drop the pointer before svr is destroyed.
      g_http_svr = nullptr;                                        // BU: stop() becomes a no-op after this.
    }
    running_.store(false);                                         // BU: Listen ended (stop or bind failure).
  });
}

}  // namespace iwars
