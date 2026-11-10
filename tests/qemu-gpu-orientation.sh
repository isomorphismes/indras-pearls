#!/usr/bin/env bash
# An independently generated, source-backed CPU fixture must match decoded
# framebuffer geometry on a rectangular emulator, not just total black pixels.
set -euo pipefail
screen="${1:?PNG from installed signed APK required}"
oracle="${2:?ICK-compiled CPU fixture executable required}"
out="${3:-qemu-orientation}"
mkdir -p "$out"
printf 'state\tstarted\n' > "$out/status.tsv"
test -s "$screen"
test -x "$oracle"
size="$(identify -format '%wx%h' "$screen")"
width="${size%x*}"
height="${size#*x}"
test "$width" -ne "$height"
"$oracle" "$width" "$height" "$out/correct.ppm" "$out/old-permuted.ppm" \
    | tee "$out/oracle.log"
# Exclude the two top controls, as well as the bottom Android gesture strip.
crop_y=$((height / 4))
crop_h=$((height / 2))
for item in correct old-permuted; do
    convert "$out/$item.ppm" \
        -crop "${width}x${crop_h}+0+${crop_y}" +repage \
        "$out/$item-crop.png"
done
convert "$screen" -crop "${width}x${crop_h}+0+${crop_y}" +repage \
    "$out/actual-crop.png"
metric() {
    local measurement
    measurement="$(compare -metric AE -fuzz 2% -channel RGB \
        "$out/actual-crop.png" "$out/$1-crop.png" null: 2>&1 || :)"
    # ImageMagick 7 may report "N (N)" rather than just "N".
    measurement="${measurement%% *}"
    case "$measurement" in
        ''|*[!0-9]*) echo "bad AE metric for $1: $measurement" >&2; return 1;;
    esac
    printf '%s\n' "$measurement"
}
correct_error="$(metric correct)"
old_error="$(metric old-permuted)"
printf 'correct_AE\t%s\nold_permuted_AE\t%s\n' \
    "$correct_error" "$old_error" > "$out/orientation.tsv"
# Deliberate incorrect-layout negative control: a geometrically wrong upload
# MUST be much less accurate than the displayed row-contiguous reference.
test "$old_error" -gt 50
test "$((correct_error * 3 + 20))" -lt "$old_error"
printf 'state\tPASS\nsize\t%s\ncorrect_AE\t%s\nold_permuted_AE\t%s\n' \
    "$size" "$correct_error" "$old_error" > "$out/status.tsv"
