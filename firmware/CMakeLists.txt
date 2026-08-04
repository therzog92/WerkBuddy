cmake_minimum_required(VERSION 3.16)
project(werkpager_sim C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WP_HOR_RES 480)
set(WP_VER_RES 480)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/lvgl)

add_executable(werkpager_sim
  src/main.cpp
  src/app/app.cpp
  src/app/storage.cpp
  src/net/sim_link.cpp
  src/ui/theme.cpp
  src/ui/chrome.cpp
  src/ui/icons.cpp
  src/ui/fonts.cpp
  src/ui/emoji_badge.cpp
  src/ui/brightness.cpp
  src/ui/nav.cpp
  src/ui/scr_hub.cpp
  src/ui/scr_pager.cpp
  src/ui/scr_games.cpp
  src/ui/scr_doodle.cpp
  src/ui/scr_settings.cpp
  src/ui/scr_idle.cpp
  src/ui/scr_splash.cpp
  src/sim/screenshot.cpp
)

target_include_directories(werkpager_sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${SDL2_INCLUDE_DIRS}
)

target_compile_definitions(werkpager_sim PRIVATE
  WP_HOR_RES=${WP_HOR_RES}
  WP_VER_RES=${WP_VER_RES}
  SDL_MAIN_HANDLED
)

target_link_libraries(werkpager_sim PRIVATE lvgl ${SDL2_LIBRARIES})
target_link_options(werkpager_sim PRIVATE ${SDL2_LDFLAGS_OTHER})

if(WIN32)
  target_link_libraries(werkpager_sim PRIVATE mingw32)
endif()

add_custom_command(TARGET werkpager_sim POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/sim-out"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/emoji"
          "${CMAKE_BINARY_DIR}/assets/emoji"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/fonts"
          "${CMAKE_BINARY_DIR}/assets/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/fx"
          "${CMAKE_BINARY_DIR}/assets/fx"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/memory"
          "${CMAKE_BINARY_DIR}/assets/memory"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/icons"
          "${CMAKE_BINARY_DIR}/assets/icons"
)
