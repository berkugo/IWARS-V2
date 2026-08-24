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

class WsServer;  // BU: Forward declare so HttpServer can hold a pointer without including the WS impl.

// BU: REST API (cpp-httplib) that mutates the engine and lists/saves scenario files.
class HttpServer {
 public:
  HttpServer(Engine& engine, UdpSender& udp, std::string scenarios_dir,
             std::string web_root, int port = 8080, int ws_port = 8081);
  ~HttpServer();                // BU: Stop the listen thread on destruction.

  void start();                 // BU: Spawn the thread that registers routes and calls listen().
  void stop();                  // BU: Ask httplib to stop and join the thread.
  void set_ws(WsServer* ws);    // BU: Optional WS pointer (currently stored; broadcasts go via Engine callback).

 private:
  Engine& engine_;              // BU: Simulation engine this API drives.
  UdpSender& udp_;              // BU: UDP sender reconfigured when /api/udp or a scenario load changes destination.
  std::string scenarios_dir_;   // BU: Directory scanned/written by /api/scenarios.
  std::string web_root_;        // BU: Optional frontend/dist to serve the UI from this process (empty = API only).
  int port_;                    // BU: HTTP listen port (default 8080).
  int ws_port_;                 // BU: Advertised WS port so the bundled UI can connect without Vite.
  WsServer* ws_{nullptr};       // BU: Optional live-feed server pointer.
  std::atomic<bool> running_{false};  // BU: True while the listen thread is supposed to be up.
  std::thread thread_;          // BU: Background thread running httplib::Server::listen.
};

// BU: Minimal RFC6455 server that accepts browsers and broadcasts JSON text frames.
class WsServer {
 public:
  explicit WsServer(int port = 8081);  // BU: Remember the listen port (default 8081).
  ~WsServer();                         // BU: Stop accept loop and close clients.

  void start();                        // BU: Create/bind/listen the TCP socket and spawn accept_loop.
  void stop();                         // BU: Close listen + client fds and join the accept thread.
  void broadcast(const std::string& message);  // BU: Send a text frame to every connected client; drop failures.

 private:
  int port_;                           // BU: TCP port to listen on.
  std::atomic<bool> running_{false};   // BU: Lifetime flag for accept and per-client read loops.
  int listen_fd_{-1};                  // BU: Listening TCP socket, or -1 when stopped.
  mutable std::mutex clients_mu_;      // BU: Guards the connected-client fd list.
  std::vector<int> clients_;           // BU: Live client sockets after a successful handshake.
  std::thread accept_thread_;          // BU: Thread blocked in accept(2).

  void accept_loop();                  // BU: accept() forever and detach a handler per client.
  void handle_client(int fd);          // BU: Handshake, then read/discard frames until close/ping.
  static bool do_handshake(int fd, const std::string& request);  // BU: RFC6455 opening handshake (101 + Accept key).
  static bool send_frame(int fd, const std::string& payload);    // BU: Build an unmasked text frame and send it.
};

}  // namespace iwars
