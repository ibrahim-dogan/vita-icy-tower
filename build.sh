#!/usr/bin/env bash
# Icy Tower Vita build helper. Everything runs inside Docker.
#
#   ./build.sh            build dist/icytower.vpk (PS Vita)
#   ./build.sh test       replay compatibility test + headless screenshot runs
#   ./build.sh livearea   regenerate the LiveArea images
#   ./build.sh media      README screenshots and trailer (tools/make_media.sh)
#
# Tests and screenshots need the original game files in ./gamedata
# (see tools/prepare_data.sh). They are never part of the VPK.
set -euo pipefail
cd "$(dirname "$0")"

TOOLS_IMAGE=icytower-tools
VITA_IMAGE=vitasdk/vitasdk:latest

tools() {
  docker build -q -t "$TOOLS_IMAGE" -f docker/Dockerfile.tools docker >/dev/null
  docker run --rm -v "$PWD":/work -w /work "$TOOLS_IMAGE" "$@"
}

case "${1:-vpk}" in
  vpk)
    docker run --rm -v "$PWD":/work -w /work "$VITA_IMAGE" sh -c \
      'cmake -S . -B build-vita -DICYTOWER_VITA=ON >/dev/null && cmake --build build-vita -j'
    mkdir -p dist
    cp build-vita/icytower.vpk dist/icytower.vpk
    echo "built dist/icytower.vpk"
    ;;
  test)
    tools sh -c 'cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build-host -j >/dev/null'
    tools ./build-host/replay_test gamedata/replays/examples/*.itr || true
    mkdir -p shots
    for s in tools/shots/*.txt; do
      echo "running $s"
      tools sh -c "SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ICYTOWER_DATA=gamedata ICYTOWER_CONFIG=/tmp/t.cfg ICYTOWER_SCRIPT=$s ${ICYTOWER_ENV:-} ./build-host/icytower"
    done
    ;;
  livearea)
    tools python3 tools/gen_livearea.py
    ;;
  media)
    tools/make_media.sh
    ;;
  *)
    echo "usage: $0 [vpk|test|livearea|media]" >&2
    exit 1
    ;;
esac
