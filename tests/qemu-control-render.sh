#!/usr/bin/env bash
set -euo pipefail

apk="${1:?APK path is required}"
out="${2:-qemu-control}"
package="org.isomorphisms.indraspearls"

mkdir -p "$out"
printf 'state\tstarted\n' > "$out/status.tsv"
test -f "$apk"

adb install -r "$apk" | tee "$out/install.txt"
adb shell run-as "$package" mkdir -p files
adb shell run-as "$package" touch files/frame-probe.enable
adb shell input keyevent KEYCODE_WAKEUP
adb shell wm dismiss-keyguard
adb shell svc power stayon true
adb shell am force-stop "$package"
adb shell am start -W \
    -n "$package/android.app.NativeActivity" \
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
adb shell dumpsys activity activities > "$out/activity.txt"
grep -F "$package" "$out/activity.txt"

adb logcat -d -s IndrasPearls:I > "$out/initial-logcat.txt"
grep -E 'Schottky radius control r=0\.700' "$out/initial-logcat.txt"
grep -E 'QEMU frame probe r=0\.700000 hash=[0-9a-f]+' "$out/initial-logcat.txt"

before_hash="$(sed -n 's/.*QEMU frame probe r=0\.700000 hash=\([0-9a-f][0-9a-f]*\).*/\1/p' "$out/initial-logcat.txt" | tail -n 1)"
test -n "$before_hash"
adb exec-out run-as "$package" cat files/frame-probe.ppm > "$out/before-gpu.ppm"
test -s "$out/before-gpu.ppm"
convert "$out/before-gpu.ppm" "$out/before-gpu.png"
adb exec-out screencap -p > "$out/before-display.png"

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
adb logcat -d > "$out/logcat.txt"

grep -E 'IndrasPearls.*Schottky radius control r=0\.400' "$out/logcat.txt"
grep -E 'IndrasPearls.*QEMU frame probe r=0\.400000 hash=[0-9a-f]+' "$out/logcat.txt"

after_hash="$(sed -n 's/.*QEMU frame probe r=0\.400000 hash=\([0-9a-f][0-9a-f]*\).*/\1/p' "$out/logcat.txt" | tail -n 1)"
test -n "$after_hash"
test "$before_hash" != "$after_hash"

adb exec-out run-as "$package" cat files/frame-probe.ppm > "$out/after-gpu.ppm"
test -s "$out/after-gpu.ppm"
convert "$out/after-gpu.ppm" "$out/after-gpu.png"
adb exec-out screencap -p > "$out/after-display.png"

probe_size="$(identify -format '%wx%h' "$out/before-gpu.ppm")"
test "$probe_size" = "$(identify -format '%wx%h' "$out/after-gpu.ppm")"

compare_status=0
if changed_pixels="$(compare -metric AE -fuzz 2% \
    "$out/before-gpu.ppm" \
    "$out/after-gpu.ppm" null: 2>&1)"; then
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

probe_width="${probe_size%x*}"
probe_height="${probe_size#*x}"
minimum_changed=$((probe_width * probe_height / 1000))
echo "before_hash=$before_hash after_hash=$after_hash"
echo "changed_pixels=$changed_pixels minimum_required=$minimum_changed"
test "$changed_pixels" -gt "$minimum_changed"

{
    printf 'state\tPASS\n'
    printf 'radius_before\t0.700\n'
    printf 'radius_after\t0.400\n'
    printf 'frame_hash_before\t%s\n' "$before_hash"
    printf 'frame_hash_after\t%s\n' "$after_hash"
    printf 'changed_pixels\t%s\n' "$changed_pixels"
    printf 'minimum_changed_pixels\t%s\n' "$minimum_changed"
    printf 'probe_size\t%s\n' "$probe_size"
} > "$out/status.tsv"
