#pragma once

#include "iwars/net/udp_sender.hpp"
#include "iwars/sim/engine.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace iwars {

class WsServer;

class HttpServer {
 public:
  HttpServer(Engine& engine, UdpSender& udp, std::string scenarios_dir,
             int port = 8080);
  ~HttpServer();

  void start();
  void stop();
  void set_ws(WsServer* ws);

 private:
  Engine& engine_;
  UdpSender& udp_;
  std::string scenarios_dir_;
  int port_;
  WsServer* ws_{nullptr};
  std::atomic<bool> running_{false};
  std::thread thread_;
};

class WsServer {
 public:
  explicit WsServer(int port = 8081);
  ~WsServer();

  void start();
  void stop();
  void broadcast(const std::string& message);

 private:
  int port_;
  std::atomic<bool> running_{false};
  int listen_fd_{-1};
  mutable std::mutex clients_mu_;
  std::vector<int> clients_;
  std::thread accept_thread_;

  void accept_loop();
  void handle_client(int fd);
  static bool do_handshake(int fd, const std::string& request);
  static bool send_frame(int fd, const std::string& payload);
};

}  // namespace iwars
