cmake_minimum_required(VERSION 3.14)

project(kickC LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(kickC
  main.cpp
  client.h
  client.cpp
)
target_link_libraries(kickC)

install(TARGETS kickC
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
