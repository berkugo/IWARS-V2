cmake_minimum_required(VERSION 3.20)
project(iwars_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(iwars_sim
  src/main.cpp
  src/sim/engine.cpp
  src/sim/scenario.cpp
  src/net/udp_sender.cpp
  src/net/packet_encoder.cpp
  src/api/http_server.cpp
  src/api/ws_server.cpp
)

target_include_directories(iwars_sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(iwars_sim PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
  Threads::Threads
)

if(WIN32)
  target_link_libraries(iwars_sim PRIVATE ws2_32)
endif()

target_compile_definitions(iwars_sim PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT=0)
