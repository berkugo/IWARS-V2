# BU: Require a CMake new enough for C++20 targets and imported OpenSSL/Threads packages.
cmake_minimum_required(VERSION 3.20)
# BU: Name the project and restrict it to C++ (no C/Fortran).
project(iwars_sim LANGUAGES CXX)

# BU: Compile as C++20 so we can use designated inits, std::filesystem, and atomics cleanly.
set(CMAKE_CXX_STANDARD 20)
# BU: Fail the configure step if the compiler cannot do C++20.
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# BU: Emit compile_commands.json for clangd / IDE indexing.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# BU: OpenSSL is used by the WebSocket handshake (SHA-1 of the Sec-WebSocket-Key).
find_package(OpenSSL REQUIRED)
# BU: pthread / Win32 threads for the engine, HTTP, and WS worker threads.
find_package(Threads REQUIRED)

# BU: Build a single executable from the engine, net, and API translation units.
add_executable(iwars_sim
  src/main.cpp
  src/sim/engine.cpp
  src/sim/scenario.cpp
  src/net/udp_sender.cpp
  src/net/packet_encoder.cpp
  src/api/http_server.cpp
  src/api/ws_server.cpp
)

# BU: Public headers live in include/; vendored httplib and nlohmann live in third_party/.
target_include_directories(iwars_sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# BU: Link SSL/crypto for SHA-1 and the platform thread library.
target_link_libraries(iwars_sim PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
  Threads::Threads
)

# BU: On Windows the UDP/TCP sockets come from Winsock, not libc.
if(WIN32)
  target_link_libraries(iwars_sim PRIVATE ws2_32)
endif()

# BU: cpp-httplib is used as a plain HTTP server; do not pull OpenSSL into httplib itself.
target_compile_definitions(iwars_sim PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT=0)
