# The same app/ sources the simulator compiles: feed, game, demo, UI and fonts.
set(APP_DIR "${CMAKE_CURRENT_LIST_DIR}/../../app")
file(GLOB APP_SRC "${APP_DIR}/src/*.c" "${APP_DIR}/fonts/*.c")

idf_component_register(
    SRCS "main.c" "board.c" "ble.c" ${APP_SRC}
    INCLUDE_DIRS "." "${APP_DIR}/include"
    REQUIRES esp_lcd esp_driver_i2c esp_driver_gpio esp_timer esp_psram bt nvs_flash
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-parameter)
