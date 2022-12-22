cmake_minimum_required(VERSION 3.24)
project(card_game)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(thirdparty/raylib)
add_subdirectory(thirdparty/raygui)
add_subdirectory(thirdparty/stduuid)

add_executable(card_game src/main.cpp thirdparty/raygui/src/raygui.h)
target_link_libraries(card_game raylib)
target_link_libraries(card_game raygui)
target_link_libraries(card_game stduuid)
