#!/usr/bin/env bash
set -euo pipefail

apk="${1:?APK path is required}"
out="${2:-qemu-original-traces}"
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

by_width=$((width / 6))
by_height=$((height / 10))
if [ "$by_width" -lt "$by_height" ]; then
    control_radius="$by_width"
else
    control_radius="$by_height"
fi
control_y=$((135 * control_radius / 100))

# Screen comparison excludes both trace disks near the top and Android
# navigation chrome near the bottom.
crop_y=$((height / 4))
crop_height=$((height / 2))
screen_pixels=$((width * crop_height))

capture_screen_crop() {
    label="$1"
    phase="$2"
    adb exec-out screencap -p > "$out/$phase-$label-full.png"
    test -s "$out/$phase-$label-full.png"
    convert "$out/$phase-$label-full.png" \
        -crop "${width}x${crop_height}+0+${crop_y}" +repage \
        "$out/$phase-$label.png"
    convert "$out/$phase-$label.png" -alpha off RGB:- \
        | sha256sum | awk '{print $1}'
}

launch_baseline() {
    label="$1"
    adb logcat -c
    adb shell am force-stop "$package"
    adb shell am start -W -n "$package/android.app.NativeActivity" \
        > "$out/launch-$label.txt"
    sleep 3

    adb logcat -d > "$out/before-$label.log"
    grep -F 'GLES original-raster renderer ready:' "$out/before-$label.log"
    grep -F 'original dgulotta/kleinian raster ready:' "$out/before-$label.log"
    grep -F 'original GPU frame probe' "$out/before-$label.log"

    raster_line="$(
        grep -F 'original dgulotta/kleinian raster ready:' "$out/before-$label.log" \
            | tail -n 1
    )"
    gpu_line="$(
        grep -F 'original GPU frame probe' "$out/before-$label.log" \
            | tail -n 1
    )"

    before_raster_hash="$(
        printf '%s\n' "$raster_line" \
            | sed -n 's/.* rgbhash=\([0-9a-fA-F][0-9a-fA-F]*\) .*/\1/p'
    )"
    before_gpu_hash="$(
        printf '%s\n' "$gpu_line" \
            | sed -n 's/.* rgbhash=\([0-9a-fA-F][0-9a-fA-F]*\) .*/\1/p'
    )"
    test -n "$before_raster_hash"
    test -n "$before_gpu_hash"

    before_screen_hash="$(capture_screen_crop "$label" before)"
    printf '%s' "$before_raster_hash" > "$out/before-$label.raster-hash"
    printf '%s' "$before_gpu_hash" > "$out/before-$label.gpu-hash"
    printf '%s' "$before_screen_hash" > "$out/before-$label.screen-hash"
}

check_after_drag() {
    label="$1"
    expected_trace="$2"

    sleep 3
    adb logcat -d > "$out/after-$label.log"
    grep -E "$expected_trace" "$out/after-$label.log"
    grep -F 'original GPU frame probe' "$out/after-$label.log"

    raster_line="$(
        grep -F 'original dgulotta/kleinian raster ready:' "$out/after-$label.log" \
            | tail -n 1
    )"
    gpu_line="$(
        grep -F 'original GPU frame probe' "$out/after-$label.log" \
            | tail -n 1
    )"

    after_raster_hash="$(
        printf '%s\n' "$raster_line" \
            | sed -n 's/.* rgbhash=\([0-9a-fA-F][0-9a-fA-F]*\) .*/\1/p'
    )"
    after_gpu_hash="$(
        printf '%s\n' "$gpu_line" \
            | sed -n 's/.* rgbhash=\([0-9a-fA-F][0-9a-fA-F]*\) .*/\1/p'
    )"
    test -n "$after_raster_hash"
    test -n "$after_gpu_hash"

    before_raster_hash="$(cat "$out/before-$label.raster-hash")"
    before_gpu_hash="$(cat "$out/before-$label.gpu-hash")"
    before_screen_hash="$(cat "$out/before-$label.screen-hash")"
    after_screen_hash="$(capture_screen_crop "$label" after)"

    test "$before_raster_hash" != "$after_raster_hash"
    test "$before_gpu_hash" != "$after_gpu_hash"
    test "$before_screen_hash" != "$after_screen_hash"

    compare_status=0
    if changed_pixels="$(
        compare -metric AE -fuzz 2% -channel RGB \
            "$out/before-$label.png" "$out/after-$label.png" null: 2>&1
    )"; then
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
    minimum_changed=$((screen_pixels / 500))
    test "$changed_pixels" -gt "$minimum_changed"

    rmse_status=0
    if rmse_metric="$(
        compare -metric RMSE -channel RGB \
            "$out/before-$label.png" "$out/after-$label.png" null: 2>&1
    )"; then
        rmse_status=0
    else
        rmse_status=$?
    fi
    test "$rmse_status" -eq 1
    normalized_rmse="$(
        printf '%s\n' "$rmse_metric" \
            | sed -n 's/.*(\([0-9.eE+-][0-9.eE+-]*\)).*/\1/p'
    )"
    test -n "$normalized_rmse"
    awk -v value="$normalized_rmse" 'BEGIN { exit !(value > 0.005) }'

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$label" \
        "$before_raster_hash" "$after_raster_hash" \
        "$before_gpu_hash" "$after_gpu_hash" \
        "$before_screen_hash" "$after_screen_hash" \
        "$changed_pixels" "$normalized_rmse" \
        >> "$out/results.tsv"
}

printf 'control\tbefore_raster_hash\tafter_raster_hash\tbefore_gpu_hash\tafter_gpu_hash\tbefore_screen_hash\tafter_screen_hash\tchanged_pixels\tnormalized_rmse\n' \
    > "$out/results.tsv"

# tr(a): move the first complex control +0.5 along the real axis.
launch_baseline "trace-a"
center_x=$((width / 4))
adb logcat -c
adb shell input touchscreen swipe \
    "$center_x" "$control_y" \
    "$((center_x + control_radius / 2))" "$control_y" 500
check_after_drag "trace-a" \
    'original dgulotta/kleinian raster ready: tr\(a\)=\(2\.39[0-9]*,0\.00[0-9]*\)'

# tr(b): restart to defaults, then move the second complex control.
launch_baseline "trace-b"
center_x=$((3 * width / 4))
adb logcat -c
adb shell input touchscreen swipe \
    "$center_x" "$control_y" \
    "$((center_x + control_radius / 2))" "$control_y" 500
check_after_drag "trace-b" \
    'original dgulotta/kleinian raster ready: .*tr\(b\)=\(2\.39[0-9]*,0\.00[0-9]*\)'

{
    printf 'state\tPASS\n'
    printf 'controls_tested\t2\n'
    printf 'semantic_coordinates_tested\ttr(a),tr(b) real axes\n'
    printf 'screen_crop\t%sx%s+0+%s\n' "$width" "$crop_height" "$crop_y"
} > "$out/status.tsv"
