cmake_minimum_required(VERSION 3.14)

project(cli LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(9les
  main.cpp
  server.h
  server.cpp
)
target_link_libraries(9les PRIVATE Threads::Threads)

install(TARGETS 9les
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
