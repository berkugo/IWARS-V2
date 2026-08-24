# BU: Require a CMake new enough for C++20 targets and imported Threads packages.
cmake_minimum_required(VERSION 3.20)
# BU: Name the project and restrict it to C++ (no C/Fortran).
project(iwars_sim LANGUAGES CXX)

# BU: Compile as C++20 so we can use designated inits, std::filesystem, and atomics cleanly.
set(CMAKE_CXX_STANDARD 20)
# BU: Fail the configure step if the compiler cannot do C++20.
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# BU: Emit compile_commands.json for clangd / IDE indexing.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# BU: Public headers live in include/; vendored httplib, nlohmann, and SHA-1 live in third_party/.
target_include_directories(iwars_sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# BU: Platform thread library. SHA-1 is header-only (third_party/sha1.hpp); no OpenSSL.
target_link_libraries(iwars_sim PRIVATE
  Threads::Threads
)

# BU: On Windows the UDP/TCP sockets come from Winsock, not libc.
if(WIN32)
  target_link_libraries(iwars_sim PRIVATE ws2_32)
endif()

# BU: Do not define CPPHTTPLIB_OPENSSL_SUPPORT — httplib uses #ifdef, so =0 still
# BU: pulls OpenSSL. Plain HTTP only; TLS is unused.

