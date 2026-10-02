#!/usr/bin/env bash
set -euo pipefail

apk="${1:?APK path is required}"
out="${2:-qemu-control}"
package="org.isomorphisms.indraspearls"

mkdir -p "$out"
printf 'state\tstarted\n' > "$out/status.tsv"
test -f "$apk"

adb install -r "$apk" | tee "$out/install.txt"
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

# The disk centers are 1.45 radii below the top edge in Android input
# coordinates. Start half a radius farther down to avoid top-edge system
# gesture handling while remaining comfortably inside the disk.
control_y=$((145 * control_radius / 100))
gesture_y=$((control_y + control_radius / 2))

# Compare only the middle of the visible display. This removes the three
# controls themselves and the bottom system-gesture area, so a passing image
# diff means the mathematical picture changed rather than just the handle.
crop_y=$((height / 5))
crop_height=$((height * 3 / 5))

{
    printf 'ro.kernel.qemu\t'
    adb shell getprop ro.kernel.qemu | tr -d '\r'
    printf 'ro.hardware\t'
    adb shell getprop ro.hardware | tr -d '\r'
    printf 'wm.size\t%sx%s\n' "$width" "$height"
    printf 'control_radius\t%s\n' "$control_radius"
    printf 'gesture_y\t%s\n' "$gesture_y"
    printf 'display_crop\t%sx%s+0+%s\n' "$width" "$crop_height" "$crop_y"
} > "$out/environment.tsv"

grep -F $'ro.kernel.qemu\t1' "$out/environment.tsv"

capture_picture() {
    label="$1"
    phase="$2"

    adb exec-out screencap -p > "$out/$phase-$label-full.png"
    test -s "$out/$phase-$label-full.png"

    dimensions="$(identify -format '%wx%h' "$out/$phase-$label-full.png")"
    test "$dimensions" = "${width}x${height}"

    convert "$out/$phase-$label-full.png" \
        -crop "${width}x${crop_height}+0+${crop_y}" +repage \
        "$out/$phase-$label.png"
    test -s "$out/$phase-$label.png"

    maximum="$(convert "$out/$phase-$label.png" -format '%[fx:maxima]' info:)"
    test "$maximum" != "0"

    sha256sum "$out/$phase-$label.png" | awk '{print $1}'
}

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

    before_hash="$(capture_picture "$label" before)"
    printf '%s' "$before_hash" > "$out/before-$label.hash"
}

check_after_drag() {
    label="$1"
    expected="$2"

    sleep 2
    adb logcat -d > "$out/after-$label.log"
    grep -E "$expected" "$out/after-$label.log"

    before_hash="$(cat "$out/before-$label.hash")"
    after_hash="$(capture_picture "$label" after)"
    test "$before_hash" != "$after_hash"

    compare_status=0
    if changed_pixels="$(compare -metric AE -fuzz 2% \
        "$out/before-$label.png" "$out/after-$label.png" null: 2>&1)"; then
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

    minimum_changed=$((width * crop_height / 2000))
    echo "$label before=$before_hash after=$after_hash changed_pixels=$changed_pixels minimum=$minimum_changed"
    test "$changed_pixels" -gt "$minimum_changed"

    printf '%s\t%s\t%s\t%s\n' \
        "$label" "$before_hash" "$after_hash" "$changed_pixels" \
        >> "$out/results.tsv"
}

printf 'control\tbefore_hash\tafter_hash\tchanged_pixels\n' > "$out/results.tsv"

# Disk 0: move the A-circle pair.
launch_baseline "u0"
center_x=$((width / 6))
adb logcat -c
adb shell input touchscreen swipe \
    "$center_x" "$gesture_y" \
    "$((center_x + 45 * control_radius / 100))" "$gesture_y" 500
check_after_drag "u0" \
    'IndrasPearls.*Schottky controls u0=\(0\.4[0-9]*,-0\.5[0-9]*\)'

# Disk 1: move the B-circle pair.
launch_baseline "u1"
center_x=$((width / 2))
adb logcat -c
adb shell input touchscreen swipe \
    "$center_x" "$gesture_y" \
    "$((center_x + 45 * control_radius / 100))" "$gesture_y" 500
check_after_drag "u1" \
    'IndrasPearls.*Schottky controls .*u1=\(0\.4[0-9]*,-0\.5[0-9]*\)'

# Disk 2: change both generator pairing phases.
launch_baseline "u2"
center_x=$((5 * width / 6))
adb logcat -c
adb shell input touchscreen swipe \
    "$center_x" "$gesture_y" \
    "$((center_x + 45 * control_radius / 100))" "$gesture_y" 500
check_after_drag "u2" \
    'IndrasPearls.*Schottky controls .*u2=\(0\.4[0-9]*,-0\.5[0-9]*\)'

{
    printf 'state\tPASS\n'
    printf 'controls_tested\t3\n'
    printf 'visible_display_crop\t%sx%s+0+%s\n' "$width" "$crop_height" "$crop_y"
    printf 'host_real_coordinates_tested\t6\n'
} > "$out/status.tsv"
