#!/bin/sh
# Build and run the simulator. The build directory lives on the local disk
# (not the SMB share) so compiling LVGL stays fast and doesn't wedge the mount.
#   sim/run.sh            build + open the window
#   sim/run.sh --shots    build + write state screenshots to docs/screens/
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="${BACKSEAT_BUILD_DIR:-$HOME/.cache/backseat-sim-build}"
cmake -S "$HERE" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu)"
if [ "$1" = "--shots" ]; then
  OUT="$HERE/../docs/screens"; mkdir -p "$OUT"
  "$BUILD/backseat_sim" --shots "$OUT"
  for f in "$OUT"/*.bmp; do sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null && rm "$f"; done
else
  exec "$BUILD/backseat_sim"
fi
