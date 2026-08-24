#include "iwars/api/server.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <mutex>

namespace fs = std::filesystem;

namespace iwars {

namespace {
nlohmann::json udp_to_json(const UdpConfig& c) {
  return nlohmann::json{
      {"host", c.host}, {"port", c.port}, {"enabled", c.enabled}};
}

std::mutex g_http_mu;
httplib::Server* g_http_svr = nullptr;
}  // namespace

HttpServer::HttpServer(Engine& engine, UdpSender& udp, std::string scenarios_dir,
                       int port)
    : engine_(engine),
      udp_(udp),
      scenarios_dir_(std::move(scenarios_dir)),
      port_(port) {}

HttpServer::~HttpServer() { stop(); }

void HttpServer::set_ws(WsServer* ws) { ws_ = ws; }

void HttpServer::stop() {
  {
    std::lock_guard lock(g_http_mu);
    if (g_http_svr) g_http_svr->stop();
  }
  if (thread_.joinable()) thread_.join();
  running_.store(false);
}

void HttpServer::start() {
  if (running_.exchange(true)) return;
  thread_ = std::thread([this] {
    httplib::Server svr;
    {
      std::lock_guard lock(g_http_mu);
      g_http_svr = &svr;
    }

    auto cors = [](const httplib::Request&, httplib::Response& res) {
      res.set_header("Access-Control-Allow-Origin", "*");
      res.set_header("Access-Control-Allow-Headers", "Content-Type");
      res.set_header("Access-Control-Allow-Methods",
                     "GET, POST, PUT, DELETE, OPTIONS");
    };

    svr.Options(R"(/.*)",
                [&](const httplib::Request& req, httplib::Response& res) {
                  cors(req, res);
                  res.status = 204;
                });

    svr.Get("/api/health",
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);
              res.set_content(R"({"ok":true})", "application/json");
            });

    svr.Get("/api/state",
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);
              res.set_content(engine_.snapshot().dump(), "application/json");
            });

    svr.Post("/api/control/play",
             [&](const httplib::Request& req, httplib::Response& res) {
               cors(req, res);
               engine_.play();
               res.set_content(R"({"playing":true})", "application/json");
             });

    svr.Post("/api/control/pause",
             [&](const httplib::Request& req, httplib::Response& res) {
               cors(req, res);
               engine_.pause();
               res.set_content(R"({"playing":false})", "application/json");
             });

    svr.Post("/api/control/reset",
             [&](const httplib::Request& req, httplib::Response& res) {
               cors(req, res);
               engine_.pause();
               engine_.reset();
               res.set_content(engine_.snapshot().dump(), "application/json");
             });

    svr.Put("/api/udp",
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);
              try {
                auto j = nlohmann::json::parse(req.body);
                UdpConfig cfg;
                cfg.host = j.value("host", std::string{"127.0.0.1"});
                cfg.port = j.value("port", 9000);
                cfg.enabled = j.value("enabled", false);
                engine_.set_udp(cfg);
                udp_.configure(cfg);
                res.set_content(udp_to_json(cfg).dump(), "application/json");
              } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(nlohmann::json{{"error", e.what()}}.dump(),
                                "application/json");
              }
            });

    svr.Post("/api/entities",
             [&](const httplib::Request& req, httplib::Response& res) {
               cors(req, res);
               try {
                 Entity e = nlohmann::json::parse(req.body).get<Entity>();
                 if (e.id.empty()) {
                   res.status = 400;
                   res.set_content(R"({"error":"id required"})",
                                   "application/json");
                   return;
                 }
                 if (!engine_.add_entity(std::move(e))) {
                   res.status = 409;
                   res.set_content(R"({"error":"id exists"})",
                                   "application/json");
                   return;
                 }
                 res.set_content(engine_.snapshot().dump(), "application/json");
               } catch (const std::exception& ex) {
                 res.status = 400;
                 res.set_content(nlohmann::json{{"error", ex.what()}}.dump(),
                                 "application/json");
               }
             });

    svr.Put(R"(/api/entities/([^/]+))",
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);
              try {
                Entity e = nlohmann::json::parse(req.body).get<Entity>();
                e.id = req.matches[1];
                if (!engine_.update_entity(e)) {
                  res.status = 404;
                  res.set_content(R"({"error":"not found"})",
                                  "application/json");
                  return;
                }
                res.set_content(engine_.snapshot().dump(), "application/json");
              } catch (const std::exception& ex) {
                res.status = 400;
                res.set_content(nlohmann::json{{"error", ex.what()}}.dump(),
                                "application/json");
              }
            });

    svr.Delete(R"(/api/entities/([^/]+))",
               [&](const httplib::Request& req, httplib::Response& res) {
                 cors(req, res);
                 if (!engine_.remove_entity(req.matches[1])) {
                   const std::string id = req.matches[1];
                   res.status = (id == iwars::kOwnshipId) ? 403 : 404;
                   res.set_content(
                       id == iwars::kOwnshipId
                           ? R"({"error":"ownship AEWC737 cannot be removed"})"
                           : R"({"error":"not found"})",
                       "application/json");
                   return;
                 }
                 res.set_content(engine_.snapshot().dump(), "application/json");
               });

    svr.Get("/api/scenarios",
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);
              nlohmann::json list = nlohmann::json::array();
              std::error_code ec;
              if (fs::exists(scenarios_dir_, ec)) {
                for (const auto& entry :
                     fs::directory_iterator(scenarios_dir_)) {
                  if (entry.path().extension() == ".json") {
                    list.push_back(entry.path().filename().string());
                  }
                }
              }
              res.set_content(list.dump(), "application/json");
            });

    svr.Get(R"(/api/scenarios/([^/]+))",
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);
              const std::string name = req.matches[1];
              const fs::path path = fs::path(scenarios_dir_) / name;
              Scenario sc;
              if (!sc.load_file(path.string())) {
                res.status = 404;
                res.set_content(R"({"error":"not found"})", "application/json");
                return;
              }
              engine_.replace_scenario(sc);
              udp_.configure(sc.udp);
              res.set_content(engine_.snapshot().dump(), "application/json");
            });

    svr.Post("/api/scenarios",
             [&](const httplib::Request& req, httplib::Response& res) {
               cors(req, res);
               try {
                 auto j = nlohmann::json::parse(req.body);
                 std::string filename = j.value("filename", std::string{});
                 if (filename.empty()) {
                   filename = j.value("name", "untitled") + ".json";
                 }
                 if (filename.find("..") != std::string::npos) {
                   res.status = 400;
                   res.set_content(R"({"error":"bad filename"})",
                                   "application/json");
                   return;
                 }
                 Scenario sc = engine_.copy_scenario();
                 if (j.contains("scenario")) {
                   sc.from_json(j["scenario"]);
                   engine_.replace_scenario(sc);
                   udp_.configure(sc.udp);
                 }
                 fs::create_directories(scenarios_dir_);
                 const fs::path path = fs::path(scenarios_dir_) / filename;
                 if (!sc.save_file(path.string())) {
                   res.status = 500;
                   res.set_content(R"({"error":"write failed"})",
                                   "application/json");
                   return;
                 }
                 res.set_content(
                     nlohmann::json{{"filename", filename}, {"ok", true}}.dump(),
                     "application/json");
               } catch (const std::exception& ex) {
                 res.status = 400;
                 res.set_content(nlohmann::json{{"error", ex.what()}}.dump(),
                                 "application/json");
               }
             });

    svr.Put("/api/scenario",
            [&](const httplib::Request& req, httplib::Response& res) {
              cors(req, res);
              try {
                Scenario sc;
                sc.from_json(nlohmann::json::parse(req.body));
                engine_.replace_scenario(sc);
                udp_.configure(sc.udp);
                res.set_content(engine_.snapshot().dump(), "application/json");
              } catch (const std::exception& ex) {
                res.status = 400;
                res.set_content(nlohmann::json{{"error", ex.what()}}.dump(),
                                "application/json");
              }
            });

    std::cout << "[http] listening on 0.0.0.0:" << port_ << "\n";
    svr.listen("0.0.0.0", port_);
    {
      std::lock_guard lock(g_http_mu);
      g_http_svr = nullptr;
    }
    running_.store(false);
  });
}

}  // namespace iwars
