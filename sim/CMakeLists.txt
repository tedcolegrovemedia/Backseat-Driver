cmake_minimum_required(VERSION 3.16)
project(backseat_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- LVGL (pinned; must match firmware/main/idf_component.yml) -------------
set(LVGL_VERSION 9.5.0)
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

include(FetchContent)
FetchContent_Declare(lvgl
    URL https://github.com/lvgl/lvgl/archive/refs/tags/v${LVGL_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(lvgl)
target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(lvgl PUBLIC PkgConfig::SDL2)

# --- shared application code (same sources the firmware builds) ------------
set(APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../app")
file(GLOB APP_SRC "${APP_DIR}/src/*.c" "${APP_DIR}/fonts/*.c")

add_executable(backseat_sim main.c ${APP_SRC})
target_include_directories(backseat_sim PRIVATE "${APP_DIR}/include")
target_compile_options(backseat_sim PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_compile_definitions(backseat_sim PRIVATE BSD_UI_TEST_HOOKS)
target_link_libraries(backseat_sim PRIVATE lvgl PkgConfig::SDL2)
