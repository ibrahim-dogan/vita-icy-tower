#!/usr/bin/env bash
# README screenshots and trailer, recorded from the headless desktop build.
# Needs the original game files in ./gamedata (tools/prepare_data.sh).
#
#   tools/make_media.sh        -> docs/media/*.png, trailer.mp4, trailer.gif
set -euo pipefail
cd "$(dirname "$0")/.."
docker build -q -t icytower-tools -f docker/Dockerfile.tools docker >/dev/null
docker run --rm -v "$PWD":/work -w /work icytower-tools bash tools/media/run.sh
