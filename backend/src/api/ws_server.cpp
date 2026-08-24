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

// BU: RFC4648 Base64 encoder used to build Sec-WebSocket-Accept.
std::string base64_encode(const unsigned char* data, std::size_t len) {
  static const char* kTable =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";  // BU: Standard alphabet.
  std::string out;                                                          // BU: Encoded result.
  out.reserve(((len + 2) / 3) * 4);                                         // BU: 4 chars per 3 bytes.
  for (std::size_t i = 0; i < len; i += 3) {                                // BU: Process in 24-bit groups.
    unsigned int n = static_cast<unsigned int>(data[i]) << 16;              // BU: First byte in the high 8 of 24.
    if (i + 1 < len) n |= static_cast<unsigned int>(data[i + 1]) << 8;      // BU: Second byte if present.
    if (i + 2 < len) n |= static_cast<unsigned int>(data[i + 2]);           // BU: Third byte if present.
    out.push_back(kTable[(n >> 18) & 63]);                                  // BU: First 6 bits.
    out.push_back(kTable[(n >> 12) & 63]);                                  // BU: Next 6 bits.
    out.push_back(i + 1 < len ? kTable[(n >> 6) & 63] : '=');               // BU: Pad if we only had 1 byte.
    out.push_back(i + 2 < len ? kTable[n & 63] : '=');                      // BU: Pad if we had 1 or 2 bytes.
  }
  return out;                                                               // BU: Base64 string.
}

// BU: RFC6455 accept key = Base64( SHA1( client_key + magic GUID ) ).
std::string ws_accept_key(const std::string& client_key) {
  const std::string src =
      client_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";  // BU: Concatenate key with the protocol GUID.
  unsigned char hash[SHA_DIGEST_LENGTH];                    // BU: 20-byte SHA-1 digest.
  SHA1(reinterpret_cast<const unsigned char*>(src.data()), src.size(), hash);  // BU: OpenSSL SHA-1.
  return base64_encode(hash, SHA_DIGEST_LENGTH);            // BU: Handshake response value.
}

// BU: Extract a HTTP header value (case-insensitive fallback) from the raw request text.
std::string header_value(const std::string& req, const std::string& name) {
  const std::string key = name + ":";                       // BU: "Sec-WebSocket-Key:" etc.
  auto pos = req.find(key);                                 // BU: Fast path: exact case.
  if (pos == std::string::npos) {                           // BU: Browsers may send mixed case.
    // case-insensitive light search
    std::string lower = req;                                // BU: Copy to lowercase for search.
    std::string lkey = key;                                 // BU: Lowercased header name.
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);  // BU: Lower the request.
    std::transform(lkey.begin(), lkey.end(), lkey.begin(), ::tolower);     // BU: Lower the key.
    pos = lower.find(lkey);                                 // BU: Find in the lowered copy.
    if (pos == std::string::npos) return {};                // BU: Header missing.
  }
  pos = req.find(':', pos);                                 // BU: Colon in the original (not lowered) request.
  if (pos == std::string::npos) return {};                  // BU: Malformed line.
  ++pos;                                                    // BU: Skip the colon.
  while (pos < req.size() && (req[pos] == ' ' || req[pos] == '\t')) ++pos;  // BU: Skip OWS.
  auto end = req.find("\r\n", pos);                         // BU: End of the header line.
  if (end == std::string::npos) end = req.size();           // BU: Last line without CRLF.
  return req.substr(pos, end - pos);                        // BU: Trimmed header value.
}

}  // namespace

WsServer::WsServer(int port) : port_(port) {}  // BU: Remember listen port; socket is created in start().

WsServer::~WsServer() { stop(); }  // BU: Close fds and join accept on destruction.

void WsServer::start() {
  if (running_.exchange(true)) return;  // BU: Already listening.

  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);  // BU: IPv4 TCP listen socket.
  if (listen_fd_ < 0) {                            // BU: Kernel refused.
    running_.store(false);                         // BU: Roll back the flag.
    std::cerr << "[ws] socket failed\n";           // BU: Log bind-time failure.
    return;                                        // BU: No accept thread.
  }
  int yes = 1;                                     // BU: SO_REUSEADDR value.
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));  // BU: Allow quick restart on the same port.

  sockaddr_in addr{};                              // BU: Bind address.
  addr.sin_family = AF_INET;                       // BU: IPv4.
  addr.sin_addr.s_addr = INADDR_ANY;               // BU: All interfaces (Vite proxy + remote browsers).
  addr.sin_port = htons(static_cast<uint16_t>(port_));  // BU: Port in network order.
  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {  // BU: Claim the port.
    std::cerr << "[ws] bind failed on " << port_ << "\n";  // BU: Port in use or permission denied.
    ::close(listen_fd_);                           // BU: Release the failed fd.
    listen_fd_ = -1;                               // BU: Mark closed.
    running_.store(false);                         // BU: Not running.
    return;                                        // BU: Abort start.
  }
  if (listen(listen_fd_, 16) < 0) {                // BU: Backlog of 16 pending handshakes.
    std::cerr << "[ws] listen failed\n";           // BU: Rare kernel error.
    ::close(listen_fd_);                           // BU: Cleanup.
    listen_fd_ = -1;                               // BU: Mark closed.
    running_.store(false);                         // BU: Not running.
    return;                                        // BU: Abort start.
  }

  std::cout << "[ws] listening on 0.0.0.0:" << port_ << "\n";  // BU: Ready for browsers.
  accept_thread_ = std::thread([this] { accept_loop(); });     // BU: Block in accept() on a worker.
}

void WsServer::stop() {
  running_.store(false);                   // BU: Tell accept_loop and handle_client to exit.
  if (listen_fd_ >= 0) {                   // BU: Wake accept() by shutting the listen socket.
    ::shutdown(listen_fd_, SHUT_RDWR);     // BU: Unblock accept.
    ::close(listen_fd_);                   // BU: Release the port.
    listen_fd_ = -1;                       // BU: Mark closed.
  }
  {
    std::lock_guard lock(clients_mu_);     // BU: Close every live client so their recv loops exit.
    for (int fd : clients_) {              // BU: Each post-handshake socket.
      ::shutdown(fd, SHUT_RDWR);           // BU: Unblock recv.
      ::close(fd);                         // BU: Drop the connection.
    }
    clients_.clear();                      // BU: Empty the list.
  }
  if (accept_thread_.joinable()) accept_thread_.join();  // BU: Wait for accept_loop to finish.
}

void WsServer::accept_loop() {
  while (running_.load()) {                // BU: Until stop().
    sockaddr_in cli{};                     // BU: Peer address (unused beyond accept).
    socklen_t len = sizeof(cli);           // BU: In/out length for accept.
    const int fd =
        ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&cli), &len);  // BU: Block for the next TCP client.
    if (fd < 0) {                          // BU: Error or listen socket closed.
      if (!running_.load()) break;         // BU: Expected during shutdown.
      continue;                            // BU: Transient error — keep accepting.
    }
    std::thread(&WsServer::handle_client, this, fd).detach();  // BU: Handshake + read loop on its own thread.
  }
}

bool WsServer::do_handshake(int fd, const std::string& request) {
  const auto key = header_value(request, "Sec-WebSocket-Key");  // BU: Client nonce from the Upgrade request.
  if (key.empty()) return false;                                // BU: Not a websocket handshake.
  const auto accept = ws_accept_key(key);                       // BU: Compute Sec-WebSocket-Accept.
  std::ostringstream resp;                                      // BU: HTTP 101 response.
  resp << "HTTP/1.1 101 Switching Protocols\r\n"
       << "Upgrade: websocket\r\n"
       << "Connection: Upgrade\r\n"
       << "Sec-WebSocket-Accept: " << accept << "\r\n"
       << "\r\n";
  const auto s = resp.str();                                    // BU: Wire bytes.
  return ::send(fd, s.data(), s.size(), 0) == static_cast<ssize_t>(s.size());  // BU: True if the 101 was fully sent.
}

bool WsServer::send_frame(int fd, const std::string& payload) {
  std::vector<unsigned char> frame;                             // BU: RFC6455 text frame (server → client, unmasked).
  frame.push_back(0x81);                                        // BU: FIN + opcode 1 (text).
  const auto len = payload.size();                              // BU: Payload length in bytes.
  if (len < 126) {                                              // BU: 7-bit length fits in the second header byte.
    frame.push_back(static_cast<unsigned char>(len));           // BU: Direct length.
  } else if (len <= 0xffff) {                                   // BU: 16-bit extended length.
    frame.push_back(126);                                       // BU: Marker for 2-byte length.
    frame.push_back(static_cast<unsigned char>((len >> 8) & 0xff));  // BU: Length high byte.
    frame.push_back(static_cast<unsigned char>(len & 0xff));    // BU: Length low byte.
  } else {                                                      // BU: 64-bit extended length.
    frame.push_back(127);                                       // BU: Marker for 8-byte length.
    for (int i = 7; i >= 0; --i) {                              // BU: Big-endian 64-bit length.
      frame.push_back(static_cast<unsigned char>((len >> (8 * i)) & 0xff));
    }
  }
  frame.insert(frame.end(), payload.begin(), payload.end());    // BU: UTF-8 JSON snapshot.
  return ::send(fd, frame.data(), frame.size(), MSG_NOSIGNAL) ==
         static_cast<ssize_t>(frame.size());                    // BU: MSG_NOSIGNAL avoids SIGPIPE on a dead client.
}

void WsServer::handle_client(int fd) {
  char buf[4096];                                               // BU: First-read buffer for the HTTP upgrade.
  const ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);        // BU: Read the handshake request.
  if (n <= 0) {                                                 // BU: Client hung up immediately.
    ::close(fd);                                                // BU: Drop the fd.
    return;                                                     // BU: No list entry yet.
  }
  buf[n] = '\0';                                                // BU: C-string safety (we also pass length).
  std::string req(buf, static_cast<std::size_t>(n));            // BU: Handshake as a string.
  if (!do_handshake(fd, req)) {                                 // BU: Reject non-WS clients.
    ::close(fd);                                                // BU: Close without adding to clients_.
    return;                                                     // BU: Done.
  }

  {
    std::lock_guard lock(clients_mu_);                          // BU: Register for broadcast().
    clients_.push_back(fd);                                     // BU: Live subscriber.
  }

  // Keep connection; read and discard client frames until close
  while (running_.load()) {                                     // BU: Until server stop or client close.
    unsigned char hdr[2];                                       // BU: First two bytes of a WS frame.
    const ssize_t hn = ::recv(fd, hdr, 2, MSG_WAITALL);         // BU: Block for a full 2-byte header.
    if (hn <= 0) break;                                         // BU: Disconnect.
    const bool masked = (hdr[1] & 0x80) != 0;                   // BU: Clients MUST mask; we still honor the bit.
    std::uint64_t len = hdr[1] & 0x7f;                          // BU: 7-bit length or extended marker.
    if (len == 126) {                                           // BU: Next 2 bytes are the real length.
      unsigned char ext[2];                                     // BU: 16-bit length.
      if (::recv(fd, ext, 2, MSG_WAITALL) != 2) break;          // BU: Incomplete frame.
      len = (static_cast<std::uint64_t>(ext[0]) << 8) | ext[1]; // BU: Combine BE length.
    } else if (len == 127) {                                    // BU: Next 8 bytes are the real length.
      unsigned char ext[8];                                     // BU: 64-bit length.
      if (::recv(fd, ext, 8, MSG_WAITALL) != 8) break;          // BU: Incomplete frame.
      len = 0;                                                  // BU: Accumulate BE integer.
      for (int i = 0; i < 8; ++i) len = (len << 8) | ext[i];    // BU: Big-endian fold.
    }
    unsigned char mask[4]{};                                    // BU: Masking key if present.
    if (masked) {                                               // BU: RFC6455 client frames are masked.
      if (::recv(fd, mask, 4, MSG_WAITALL) != 4) break;         // BU: Need 4 mask bytes.
    }
    std::vector<char> payload(static_cast<std::size_t>(len));   // BU: Application data (we ignore JSON from the UI).
    if (len > 0) {                                              // BU: Empty ping/pong/close may have no payload.
      if (::recv(fd, payload.data(), static_cast<size_t>(len), MSG_WAITALL) !=
          static_cast<ssize_t>(len))
        break;                                                  // BU: Short read — drop client.
      if (masked) {                                             // BU: Unmask so opcode handling could inspect payload.
        for (std::size_t i = 0; i < payload.size(); ++i)
          payload[i] ^= mask[i % 4];                            // BU: XOR with repeating 4-byte key.
      }
    }
    const unsigned opcode = hdr[0] & 0x0f;                      // BU: Frame type.
    if (opcode == 0x8) break;                                   // BU: Close frame — end the loop.
    if (opcode == 0x9) {                                        // BU: Ping — reply with pong so proxies keep the connection.
      // ping -> pong
      unsigned char pong[2] = {0x8a, 0x00};                     // BU: FIN + opcode 0xA, zero-length payload.
      ::send(fd, pong, 2, MSG_NOSIGNAL);                        // BU: Best-effort pong.
    }
  }

  {
    std::lock_guard lock(clients_mu_);                          // BU: Unregister before close.
    clients_.erase(std::remove(clients_.begin(), clients_.end(), fd),
                   clients_.end());                             // BU: Remove this fd from the broadcast list.
  }
  ::close(fd);                                                  // BU: Release the socket.
}

void WsServer::broadcast(const std::string& message) {
  std::vector<int> dead;                                        // BU: Fds that failed send_frame.
  std::lock_guard lock(clients_mu_);                            // BU: Iterate a stable snapshot of clients_.
  for (int fd : clients_) {                                     // BU: Every subscribed browser.
    if (!send_frame(fd, message)) dead.push_back(fd);           // BU: Remember peers that went away.
  }
  for (int fd : dead) {                                         // BU: Drop dead connections.
    clients_.erase(std::remove(clients_.begin(), clients_.end(), fd),
                   clients_.end());                             // BU: Remove from the list.
    ::close(fd);                                                // BU: Close the broken socket.
  }
}

}  // namespace iwars
