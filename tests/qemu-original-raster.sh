#!/usr/bin/env bash
set -euo pipefail

apk="${1:?APK path is required}"
out="${2:-qemu-original}"
package="org.isomorphisms.indraspearls"

mkdir -p "$out"
printf 'state\tstarted\n' > "$out/status.tsv"
test -f "$apk"

adb install -r "$apk" | tee "$out/install.txt"
adb shell input keyevent KEYCODE_WAKEUP
adb shell wm dismiss-keyguard
adb shell svc power stayon true
adb logcat -c
adb shell am force-stop "$package"
adb shell am start -W -n "$package/android.app.NativeActivity" \
    | tee "$out/launch.txt"
sleep 3

adb shell dumpsys window windows > "$out/window.txt"
grep -F "$package" "$out/window.txt"

adb logcat -d > "$out/logcat.txt"
grep -F 'original dgulotta/kleinian raster ready:' "$out/logcat.txt"
grep -F 'GLES original-raster renderer ready:' "$out/logcat.txt"
grep -F 'original GPU frame probe' "$out/logcat.txt"

probe_line="$(grep -F 'original GPU frame probe' "$out/logcat.txt" | tail -n 1)"
dark_pixels="$(printf '%s\n' "$probe_line" | sed -n 's/.* dark=\([0-9][0-9]*\) .*/\1/p')"
bright_pixels="$(printf '%s\n' "$probe_line" | sed -n 's/.* bright=\([0-9][0-9]*\) .*/\1/p')"
gpu_pixels="$(printf '%s\n' "$probe_line" | sed -n 's/.* pixels=\([0-9][0-9]*\).*/\1/p')"
gpu_hash="$(printf '%s\n' "$probe_line" | sed -n 's/.* rgbhash=\([0-9a-fA-F][0-9a-fA-F]*\) .*/\1/p')"

test -n "$dark_pixels"
test -n "$bright_pixels"
test -n "$gpu_pixels"
test -n "$gpu_hash"
test "$dark_pixels" -gt 500
test "$dark_pixels" -lt "$((gpu_pixels / 2))"
test "$bright_pixels" -gt "$((gpu_pixels / 2))"

adb exec-out screencap -p > "$out/display-full.png"
test -s "$out/display-full.png"

size="$(identify -format '%wx%h' "$out/display-full.png")"
width="${size%x*}"
height="${size#*x}"
test "$width" -gt 0
test "$height" -gt 0

# Exclude the bottom navigation/gesture strip.  The original raster itself
# is monochrome, so count dark pixels explicitly in decoded grayscale.
crop_height=$((height * 9 / 10))
convert "$out/display-full.png" \
    -crop "${width}x${crop_height}+0+0" +repage \
    "$out/display-app.png"

screen_pixels=$((width * crop_height))
screen_dark="$(
    convert "$out/display-app.png" -alpha off -colorspace Gray -threshold 20% \
        -format '%[fx:round((1-mean)*w*h)]' info:
)"
case "$screen_dark" in
    ''|*[!0-9]*)
        echo "unexpected visible dark-pixel metric: $screen_dark" >&2
        exit 1
        ;;
esac
test "$screen_dark" -gt 100
test "$screen_dark" -lt "$((screen_pixels / 2))"

{
    printf 'state\tPASS\n'
    printf 'gpu_rgb_hash\t%s\n' "$gpu_hash"
    printf 'gpu_dark_pixels\t%s\n' "$dark_pixels"
    printf 'gpu_bright_pixels\t%s\n' "$bright_pixels"
    printf 'gpu_pixels\t%s\n' "$gpu_pixels"
    printf 'display_dark_pixels\t%s\n' "$screen_dark"
    printf 'display_pixels\t%s\n' "$screen_pixels"
} > "$out/status.tsv"
