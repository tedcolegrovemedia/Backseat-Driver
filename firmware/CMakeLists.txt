# Backseat Driver board firmware (ESP-IDF v5.5.x, ESP32-S3-Touch-LCD-4.3).
#
# Build with firmware/build.sh: it mirrors this folder to ~/.cache/backseat-fw so
#   ESP-IDF never writes to the network share.
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(backseat_driver)
