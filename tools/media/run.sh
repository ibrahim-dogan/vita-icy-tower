#!/usr/bin/env bash
# Runs inside the icytower-tools image (see tools/make_media.sh).
set -euo pipefail
OUT=/out
MEDIA=docs/media
EX=gamedata/replays/examples
mkdir -p $OUT $MEDIA
rm -f $OUT/*.raw

cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-host -j >/dev/null

# run SCRIPT [REPLAY] [CHARACTER]: one headless session with its own config
run() {
  local cfg=$OUT/cfg_$RANDOM.cfg
  rm -f "$cfg"
  [ -n "${3:-}" ] && echo "character=$3" > "$cfg"
  env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ICYTOWER_DATA=gamedata ICYTOWER_CONFIG="$cfg" \
    ${2:+ICYTOWER_AUTOPLAY=$EX/$2.itr} ICYTOWER_SCRIPT="tools/media/$1.txt" ./build-host/icytower >/dev/null 2>&1
}

run shots
run shot_reward Arnon_Yaari_61719_480_237
run shot_hiscore Johan_Peitz_5760_287_39
run shot_dave Johan_Peitz_5760_287_39 disco_dave
run clip_menu
run clip_amazing Arnon_Yaari_12648_582_56
run clip_noway Arnon_Yaari_61719_480_237
run clip_dave Johan_Peitz_5760_287_39 disco_dave

# screenshots: optimised PNGs (they stay lossless-looking at 256 colours)
for s in title reward dave hiscore options replays; do
  pngquant --force --quality 85-100 --output $MEDIA/$s.png $OUT/$s.png
done

# clips: raw 960x544 RGB at 50 fps -> 30 fps H.264
enc() { ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s 960x544 -r 50 -i "$OUT/$1.raw" \
          -vf "fps=30,format=yuv420p" -c:v libx264 -crf 14 -preset slow "$OUT/$1.mp4"; }
for c in clip_menu clip_amazing clip_noway clip_dave; do enc $c; done

python3 tools/media/cards.py $OUT/reward.png $OUT/title_card.png $OUT/end_card.png
card() { ffmpeg -loglevel error -y -loop 1 -framerate 30 -t "$2" -i "$OUT/$1.png" \
           -vf format=yuv420p -c:v libx264 -crf 14 "$OUT/$1.mp4"; }
card title_card 2.6
card end_card 3.2

# join clips with 0.4 s crossfades, then put them into the Vita frame
# (screen at 420,178 on 1800x900). Clips are "name:start:length" (0 = all).
XF=0.4
python3 tools/vita_frame.py $OUT/frame.png >/dev/null
montage() {
  local out=$1; shift
  local inputs=() filter="" prev="[0:v]" offset=0 i=0 lens=()
  for spec in "$@"; do
    IFS=: read -r name ss len <<< "$spec"
    if [ "$len" = 0 ]; then
      len=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$OUT/$name.mp4")
      inputs+=(-i "$OUT/$name.mp4")
    else
      inputs+=(-ss "$ss" -t "$len" -i "$OUT/$name.mp4")
    fi
    lens+=("$len")
  done
  for i in $(seq 1 $(($# - 1))); do
    offset=$(python3 -c "print(round($offset + ${lens[$((i - 1))]} - $XF, 3))")
    filter+="${prev}[$i:v]xfade=transition=fade:duration=$XF:offset=$offset[v$i];"
    prev="[v$i]"
  done
  ffmpeg -loglevel error -y "${inputs[@]}" -i $OUT/frame.png \
    -filter_complex "${filter}[$#:v]${prev}overlay=420:178,format=yuv420p[out]" -map "[out]" \
    -c:v libx264 -crf 18 -preset slow -r 30 -movflags +faststart "$out"
}

# full trailer (MP4, for Discord)
montage $MEDIA/trailer.mp4 title_card:0:0 clip_menu:0:0 clip_amazing:0:0 clip_noway:0:0 clip_dave:0:0 end_card:0:0
# a shorter cut of the same shots for the README GIF (scrolling pixel art is
# expensive in GIF)
montage $OUT/short.mp4 title_card:0:1.6 clip_menu:0:2.2 clip_amazing:3.2:3.2 clip_noway:3.6:3.8 clip_dave:3.0:3.2 \
  end_card:0:2.6
ffmpeg -loglevel error -y -i $OUT/short.mp4 \
  -vf "fps=10,scale=900:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=96:stats_mode=full[p];[b][p]paletteuse=dither=none:diff_mode=rectangle" \
  $OUT/trailer.gif
gifsicle -O3 --lossy=150 $OUT/trailer.gif -o $MEDIA/trailer.gif
ls -la $MEDIA
for f in $MEDIA/trailer.mp4 $OUT/short.mp4; do ffprobe -v error -show_entries format=duration -of csv=p=0 $f; done
