#!/bin/bash
# Build/flash the board firmware from a local mirror, so ESP-IDF never writes
# to the network share (managed_components, build output, sdkconfig).
#   firmware/build.sh            build
#   firmware/build.sh flash      build + flash at 230400 (faster rates corrupt on this cable)
#   firmware/build.sh monitor    serial log
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
W="$HOME/.cache/backseat-fw"
PORT="${PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
mkdir -p "$W/firmware"
ln -sfn "$ROOT/app" "$W/app"
rsync -a --delete --exclude build --exclude managed_components --exclude dependencies.lock \
      --exclude sdkconfig --exclude factory-backup "$HERE/" "$W/firmware/"
. "$HOME/esp/esp-idf-v5.5.6/export.sh" >/dev/null
cd "$W/firmware"
case "${1:-build}" in
  build)   idf.py build ;;
  flash)   idf.py -p "$PORT" -b 230400 build flash ;;
  monitor) idf.py -p "$PORT" monitor ;;
  *)       idf.py "$@" ;;
esac
