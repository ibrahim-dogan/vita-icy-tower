#!/usr/bin/env bash
# Turns the official Icy Tower 1.3.1 installer (or an installed copy) into the
# folder the Vita port reads. The game files stay yours; nothing is uploaded.
#
#   tools/prepare_data.sh icytower13_install.exe [out_dir]
#   tools/prepare_data.sh "C:/Games/Icy Tower 1.3" [out_dir]     (installed copy)
#
# Then copy the contents of out_dir (default: ./icytower) to ux0:data/icytower/
# on the Vita, so that ux0:data/icytower/data/data.dat exists.
#
# Needs innoextract (apt/brew install innoextract) or Docker for installers.
set -euo pipefail
src=${1:?usage: $0 icytower13_install.exe|game_dir [out_dir]}
out=${2:-icytower}
mkdir -p "$out"

if [ -d "$src" ]; then
  app=$src
else
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' EXIT
  if command -v innoextract >/dev/null; then
    innoextract -s -d "$tmp" "$src"
  else
    cp "$src" "$tmp/setup.exe"
    docker run --rm -v "$tmp":/w -w /w debian:bookworm-slim sh -c \
      'apt-get update -qq >/dev/null && apt-get install -y -qq innoextract >/dev/null && innoextract -s -d /w /w/setup.exe'
  fi
  app="$tmp/app"
fi

for f in data/data.dat data/sfx13.dat; do
  [ -f "$app/$f" ] || { echo "missing $f in $src" >&2; exit 1; }
done
mkdir -p "$out/data" "$out/replays"
cp "$app/data/data.dat" "$app/data/sfx13.dat" "$out/data/"
cp -R "$app/characters" "$out/"
[ -d "$app/replays" ] && cp -R "$app/replays/." "$out/replays/"
[ -d "$app/sfx" ] && cp -R "$app/sfx" "$out/"
echo "ready: copy the contents of '$out' to ux0:data/icytower/"
