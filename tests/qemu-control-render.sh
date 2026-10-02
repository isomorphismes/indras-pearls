#!/usr/bin/env bash
set -euo pipefail

apk="${1:?APK path is required}"
out="${2:-qemu-control}"

mkdir -p "$out"
printf 'state\tstarted\n' > "$out/status.tsv"
test -f "$apk"

adb install -r "$apk" | tee "$out/install.txt"
adb shell am force-stop org.isomorphisms.indraspearls
adb shell am start -W \
    -n org.isomorphisms.indraspearls/android.app.NativeActivity \
    | tee "$out/launch.txt"
sleep 3

{
    printf 'ro.kernel.qemu\t'
    adb shell getprop ro.kernel.qemu | tr -d '\r'
    printf 'ro.hardware\t'
    adb shell getprop ro.hardware | tr -d '\r'
    printf 'wm.size\t'
    adb shell wm size | tr -d '\r' | tail -n 1
} > "$out/environment.tsv"

grep -F $'ro.kernel.qemu\t1' "$out/environment.tsv"
adb logcat -d -s IndrasPearls:I > "$out/initial-logcat.txt"
grep -E 'Schottky radius control r=0\.700' "$out/initial-logcat.txt"

adb exec-out screencap -p > "$out/before.png"

size_line="$(adb shell wm size | tr -d '\r' | tail -n 1)"
size="${size_line##*: }"
width="${size%x*}"
height="${size#*x}"
test "$width" -gt 0
test "$height" -gt 0

by_width=$((width / 8))
by_height=$((height / 12))
if [ "$by_width" -lt "$by_height" ]; then
    control_radius="$by_width"
else
    control_radius="$by_height"
fi

center_x=$((width / 2))
control_y=$((145 * control_radius / 100))
start_x=$((center_x + 94 * control_radius / 100))
end_x="$center_x"

adb logcat -c
adb shell input swipe "$start_x" "$control_y" "$end_x" "$control_y" 600
sleep 2
adb exec-out screencap -p > "$out/after.png"
adb logcat -d > "$out/logcat.txt"

grep -E 'IndrasPearls.*Schottky radius control r=0\.400' "$out/logcat.txt"

crop_y=$((height * 25 / 100))
crop_h=$((height * 65 / 100))
convert "$out/before.png" \
    -crop "${width}x${crop_h}+0+${crop_y}" +repage \
    "$out/before-fractal.png"
convert "$out/after.png" \
    -crop "${width}x${crop_h}+0+${crop_y}" +repage \
    "$out/after-fractal.png"

compare_status=0
if changed_pixels="$(compare -metric AE -fuzz 2% \
    "$out/before-fractal.png" \
    "$out/after-fractal.png" null: 2>&1)"; then
    compare_status=0
else
    compare_status=$?
fi

test "$compare_status" -eq 1
case "$changed_pixels" in
    ''|*[!0-9]*)
        echo "unexpected AE metric: $changed_pixels" >&2
        exit 1
        ;;
esac

minimum_changed=$((width * crop_h / 1000))
echo "changed_pixels=$changed_pixels minimum_required=$minimum_changed"
test "$changed_pixels" -gt "$minimum_changed"

{
    printf 'state\tPASS\n'
    printf 'radius_before\t0.700\n'
    printf 'radius_after\t0.400\n'
    printf 'changed_pixels\t%s\n' "$changed_pixels"
    printf 'minimum_changed_pixels\t%s\n' "$minimum_changed"
    printf 'crop\t%sx%s+0+%s\n' "$width" "$crop_h" "$crop_y"
} > "$out/status.tsv"
