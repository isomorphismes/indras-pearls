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
control_y=$((145 * control_radius / 100))
gesture_y=$((control_y + control_radius / 2))

{
    printf 'ro.kernel.qemu\t'
    adb shell getprop ro.kernel.qemu | tr -d '\r'
    printf 'ro.hardware\t'
    adb shell getprop ro.hardware | tr -d '\r'
    printf 'wm.size\t%sx%s\n' "$width" "$height"
    printf 'control_radius\t%s\n' "$control_radius"
    printf 'gesture_y\t%s\n' "$gesture_y"
} > "$out/environment.tsv"

grep -F $'ro.kernel.qemu\t1' "$out/environment.tsv"

launch_baseline() {
    label="$1"
    adb logcat -c
    adb shell am force-stop "$package"
    adb shell am start -W -n "$package/android.app.NativeActivity" > "$out/launch-$label.txt"
    sleep 2

    adb shell dumpsys window windows > "$out/window-$label.txt"
    grep -F "$package" "$out/window-$label.txt"

    adb logcat -d > "$out/before-$label.log"
    grep -E 'IndrasPearls.*Schottky controls u0=\(0\.0000,0\.0000\) u1=\(0\.0000,0\.0000\) u2=\(0\.0000,0\.0000\)' "$out/before-$label.log"
    grep -E 'IndrasPearls.*QEMU frame probe hash=[0-9a-f]+' "$out/before-$label.log"

    before_hash="$(sed -n 's/.*QEMU frame probe hash=\([0-9a-f][0-9a-f]*\).*/\1/p' "$out/before-$label.log" | tail -n 1)"
    test -n "$before_hash"
    printf '%s' "$before_hash" > "$out/before-$label.hash"

    adb exec-out run-as "$package" cat files/frame-probe.ppm > "$out/before-$label.ppm"
    test -s "$out/before-$label.ppm"

    nonzero="$(convert "$out/before-$label.ppm" -format '%[fx:maxima]' info:)"
    test "$nonzero" != "0"
}

check_after_drag() {
    label="$1"
    expected="$2"

    sleep 2
    adb logcat -d > "$out/after-$label.log"
    grep -E "$expected" "$out/after-$label.log"
    grep -E 'IndrasPearls.*QEMU frame probe hash=[0-9a-f]+' "$out/after-$label.log"

    before_hash="$(cat "$out/before-$label.hash")"
    after_hash="$(sed -n 's/.*QEMU frame probe hash=\([0-9a-f][0-9a-f]*\).*/\1/p' "$out/after-$label.log" | tail -n 1)"
    test -n "$after_hash"
    test "$before_hash" != "$after_hash"

    adb exec-out run-as "$package" cat files/frame-probe.ppm > "$out/after-$label.ppm"
    test -s "$out/after-$label.ppm"

    probe_size="$(identify -format '%wx%h' "$out/before-$label.ppm")"
    test "$probe_size" = "$(identify -format '%wx%h' "$out/after-$label.ppm")"

    compare_status=0
    if changed_pixels="$(compare -metric AE -fuzz 2% "$out/before-$label.ppm" "$out/after-$label.ppm" null: 2>&1)"; then
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
    minimum_changed=$((probe_width * probe_height / 2000))
    test "$changed_pixels" -gt "$minimum_changed"

    convert "$out/before-$label.ppm" "$out/before-$label.png"
    convert "$out/after-$label.ppm" "$out/after-$label.png"
    printf '%s\t%s\t%s\t%s\n' "$label" "$before_hash" "$after_hash" "$changed_pixels" >> "$out/results.tsv"
}

printf 'control\tbefore_hash\tafter_hash\tchanged_pixels\n' > "$out/results.tsv"

# Half a radius below the visual center stays well inside each disk while
# avoiding Android's top-edge system gesture area.

launch_baseline "u0"
center_x=$((width / 6))
adb logcat -c
adb shell input touchscreen swipe "$center_x" "$gesture_y" "$((center_x + 45 * control_radius / 100))" "$gesture_y" 500
check_after_drag "u0" 'IndrasPearls.*Schottky controls u0=\(0\.4[0-9]*,-0\.5[0-9]*\)'

launch_baseline "u1"
center_x=$((width / 2))
adb logcat -c
adb shell input touchscreen swipe "$center_x" "$gesture_y" "$((center_x + 45 * control_radius / 100))" "$gesture_y" 500
check_after_drag "u1" 'IndrasPearls.*Schottky controls .*u1=\(0\.4[0-9]*,-0\.5[0-9]*\)'

launch_baseline "u2"
center_x=$((5 * width / 6))
adb logcat -c
adb shell input touchscreen swipe "$center_x" "$gesture_y" "$((center_x + 45 * control_radius / 100))" "$gesture_y" 500
check_after_drag "u2" 'IndrasPearls.*Schottky controls .*u2=\(0\.4[0-9]*,-0\.5[0-9]*\)'

{
    printf 'state\tPASS\n'
    printf 'controls_tested\t3\n'
    printf 'qemu_gesture_coordinates\treal plus negative-imaginary inside each disk\n'
    printf 'host_real_coordinates_tested\t6\n'
} > "$out/status.tsv"
