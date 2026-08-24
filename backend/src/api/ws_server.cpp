#include "iwars/api/server.hpp"

#include <openssl/sha.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace iwars {
namespace {

std::string base64_encode(const unsigned char* data, std::size_t len) {
  static const char* kTable =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  for (std::size_t i = 0; i < len; i += 3) {
    unsigned int n = static_cast<unsigned int>(data[i]) << 16;
    if (i + 1 < len) n |= static_cast<unsigned int>(data[i + 1]) << 8;
    if (i + 2 < len) n |= static_cast<unsigned int>(data[i + 2]);
    out.push_back(kTable[(n >> 18) & 63]);
    out.push_back(kTable[(n >> 12) & 63]);
    out.push_back(i + 1 < len ? kTable[(n >> 6) & 63] : '=');
    out.push_back(i + 2 < len ? kTable[n & 63] : '=');
  }
  return out;
}

std::string ws_accept_key(const std::string& client_key) {
  const std::string src =
      client_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(src.data()), src.size(), hash);
  return base64_encode(hash, SHA_DIGEST_LENGTH);
}

std::string header_value(const std::string& req, const std::string& name) {
  const std::string key = name + ":";
  auto pos = req.find(key);
  if (pos == std::string::npos) {
    // case-insensitive light search
    std::string lower = req;
    std::string lkey = key;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    std::transform(lkey.begin(), lkey.end(), lkey.begin(), ::tolower);
    pos = lower.find(lkey);
    if (pos == std::string::npos) return {};
  }
  pos = req.find(':', pos);
  if (pos == std::string::npos) return {};
  ++pos;
  while (pos < req.size() && (req[pos] == ' ' || req[pos] == '\t')) ++pos;
  auto end = req.find("\r\n", pos);
  if (end == std::string::npos) end = req.size();
  return req.substr(pos, end - pos);
}

}  // namespace

WsServer::WsServer(int port) : port_(port) {}

WsServer::~WsServer() { stop(); }

void WsServer::start() {
  if (running_.exchange(true)) return;

  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    running_.store(false);
    std::cerr << "[ws] socket failed\n";
    return;
  }
  int yes = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port_));
  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "[ws] bind failed on " << port_ << "\n";
    ::close(listen_fd_);
    listen_fd_ = -1;
    running_.store(false);
    return;
  }
  if (listen(listen_fd_, 16) < 0) {
    std::cerr << "[ws] listen failed\n";
    ::close(listen_fd_);
    listen_fd_ = -1;
    running_.store(false);
    return;
  }

  std::cout << "[ws] listening on 0.0.0.0:" << port_ << "\n";
  accept_thread_ = std::thread([this] { accept_loop(); });
}

void WsServer::stop() {
  running_.store(false);
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
  }
  {
    std::lock_guard lock(clients_mu_);
    for (int fd : clients_) {
      ::shutdown(fd, SHUT_RDWR);
      ::close(fd);
    }
    clients_.clear();
  }
  if (accept_thread_.joinable()) accept_thread_.join();
}

void WsServer::accept_loop() {
  while (running_.load()) {
    sockaddr_in cli{};
    socklen_t len = sizeof(cli);
    const int fd =
        ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&cli), &len);
    if (fd < 0) {
      if (!running_.load()) break;
      continue;
    }
    std::thread(&WsServer::handle_client, this, fd).detach();
  }
}

bool WsServer::do_handshake(int fd, const std::string& request) {
  const auto key = header_value(request, "Sec-WebSocket-Key");
  if (key.empty()) return false;
  const auto accept = ws_accept_key(key);
  std::ostringstream resp;
  resp << "HTTP/1.1 101 Switching Protocols\r\n"
       << "Upgrade: websocket\r\n"
       << "Connection: Upgrade\r\n"
       << "Sec-WebSocket-Accept: " << accept << "\r\n"
       << "\r\n";
  const auto s = resp.str();
  return ::send(fd, s.data(), s.size(), 0) == static_cast<ssize_t>(s.size());
}

bool WsServer::send_frame(int fd, const std::string& payload) {
  std::vector<unsigned char> frame;
  frame.push_back(0x81);  // FIN + text
  const auto len = payload.size();
  if (len < 126) {
    frame.push_back(static_cast<unsigned char>(len));
  } else if (len <= 0xffff) {
    frame.push_back(126);
    frame.push_back(static_cast<unsigned char>((len >> 8) & 0xff));
    frame.push_back(static_cast<unsigned char>(len & 0xff));
  } else {
    frame.push_back(127);
    for (int i = 7; i >= 0; --i) {
      frame.push_back(static_cast<unsigned char>((len >> (8 * i)) & 0xff));
    }
  }
  frame.insert(frame.end(), payload.begin(), payload.end());
  return ::send(fd, frame.data(), frame.size(), MSG_NOSIGNAL) ==
         static_cast<ssize_t>(frame.size());
}

void WsServer::handle_client(int fd) {
  char buf[4096];
  const ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
  if (n <= 0) {
    ::close(fd);
    return;
  }
  buf[n] = '\0';
  std::string req(buf, static_cast<std::size_t>(n));
  if (!do_handshake(fd, req)) {
    ::close(fd);
    return;
  }

  {
    std::lock_guard lock(clients_mu_);
    clients_.push_back(fd);
  }

  // Keep connection; read and discard client frames until close
  while (running_.load()) {
    unsigned char hdr[2];
    const ssize_t hn = ::recv(fd, hdr, 2, MSG_WAITALL);
    if (hn <= 0) break;
    const bool masked = (hdr[1] & 0x80) != 0;
    std::uint64_t len = hdr[1] & 0x7f;
    if (len == 126) {
      unsigned char ext[2];
      if (::recv(fd, ext, 2, MSG_WAITALL) != 2) break;
      len = (static_cast<std::uint64_t>(ext[0]) << 8) | ext[1];
    } else if (len == 127) {
      unsigned char ext[8];
      if (::recv(fd, ext, 8, MSG_WAITALL) != 8) break;
      len = 0;
      for (int i = 0; i < 8; ++i) len = (len << 8) | ext[i];
    }
    unsigned char mask[4]{};
    if (masked) {
      if (::recv(fd, mask, 4, MSG_WAITALL) != 4) break;
    }
    std::vector<char> payload(static_cast<std::size_t>(len));
    if (len > 0) {
      if (::recv(fd, payload.data(), static_cast<size_t>(len), MSG_WAITALL) !=
          static_cast<ssize_t>(len))
        break;
      if (masked) {
        for (std::size_t i = 0; i < payload.size(); ++i)
          payload[i] ^= mask[i % 4];
      }
    }
    const unsigned opcode = hdr[0] & 0x0f;
    if (opcode == 0x8) break;  // close
    if (opcode == 0x9) {
      // ping -> pong
      unsigned char pong[2] = {0x8a, 0x00};
      ::send(fd, pong, 2, MSG_NOSIGNAL);
    }
  }

  {
    std::lock_guard lock(clients_mu_);
    clients_.erase(std::remove(clients_.begin(), clients_.end(), fd),
                   clients_.end());
  }
  ::close(fd);
}

void WsServer::broadcast(const std::string& message) {
  std::vector<int> dead;
  std::lock_guard lock(clients_mu_);
  for (int fd : clients_) {
    if (!send_frame(fd, message)) dead.push_back(fd);
  }
  for (int fd : dead) {
    clients_.erase(std::remove(clients_.begin(), clients_.end(), fd),
                   clients_.end());
    ::close(fd);
  }
}

}  // namespace iwars
