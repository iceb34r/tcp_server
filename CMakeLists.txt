cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(tcp_server)

add_executable(server server.cpp)

target_link_libraries(server liburing.a)