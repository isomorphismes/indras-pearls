#!/usr/bin/env bash
set -euo pipefail

apk="$1"
out="$2"
expected_api="$3"
grease="${4:?Qualified Grease executable required}"
admission="${5:?Pinned shared system-mode admission script required}"
package="org.isomorphisms.indraspearls"

test -s "$apk"
test -x "$grease"
test -f "$admission"
mkdir -p "$out"
printf 'state\tstarted\n' > "$out/status.tsv"

actual_api="$(adb shell getprop ro.build.version.sdk | tr -d '\r')"
test "$actual_api" = "$expected_api"
printf 'api\t%s\n' "$actual_api" > "$out/device.tsv"
# An emulator is blank on startup: install the exact APK that upstream
# signed and uploaded before asserting Activity presence or presentation.
adb install -r "$apk" | tee "$out/install.txt"
grep -Fx 'Success' "$out/install.txt"
adb shell pm path "$package" | tee "$out/package-path.txt"
grep -F 'package:' "$out/package-path.txt"
adb shell input keyevent KEYCODE_WAKEUP
adb shell wm dismiss-keyguard
adb shell svc power stayon true
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0

restore_device_configuration() {
    adb shell cmd window user-rotation free >/dev/null 2>&1 || :
    adb shell cmd window set-user-rotation free >/dev/null 2>&1 || :
    adb shell setprop debug.indras.gpu_probe 0 >/dev/null 2>&1 || :
    adb shell settings put system user_rotation 0 >/dev/null 2>&1 || :
    adb shell settings put system accelerometer_rotation 1 >/dev/null 2>&1 || :
    adb shell cmd uimode night no >/dev/null 2>&1 || :
}
trap restore_device_configuration EXIT

# cmd window performs a real WindowManager rotation lock; writing the
# settings value alone failed to rotate the API34 emulator.
rotate_emulator() {
    adb shell cmd window user-rotation lock "$1" >/dev/null 2>&1 || \
        adb shell cmd window set-user-rotation lock "$1" >/dev/null 2>&1
}

printf 'mode\tapi\tsystem_getter\tapp_qualifier\tcontrol_rgb\trotation\traster\ttraces\n' > "$out/results.tsv"

check_mode() {
    mode="$1"
    adb shell setprop debug.indras.gpu_probe 1
    rotate_emulator 0
    # Use the verified account-wide disposable-emulator mode admission.
    # Android 10 requires privileged day/night permission and user setup.
    "$grease" "$admission" "$(command -v adb)" emulator-5554 \
        "$actual_api" "$mode" "$out/admission-$mode"
    grep -F 'actual_service_and_resource_configuration' \
        "$out/admission-$mode/result.tsv"
    adb shell cmd uimode night | tr -d '\r' > "$out/system-$mode.txt"
    grep -Fx "Night mode: $mode" "$out/system-$mode.txt"
    adb shell am get-config | tr -d '\r' > "$out/config-$mode.txt"
    if [ "$mode" = yes ]; then
        grep -E '^config:.*-night(-|$)' "$out/config-$mode.txt"
    else
        grep -E '^config:.*-notnight(-|$)' "$out/config-$mode.txt"
    fi
    sleep 2

    adb logcat -c
    adb shell am force-stop "$package"
    adb shell am start -W -n "$package/android.app.NativeActivity" \
        > "$out/launch-$mode.txt"
    sleep 4
    adb logcat -d > "$out/launch-$mode.log"

    # This comes from AConfiguration_fromAssetManager in the *running*
    # native Activity. It is deliberately independent of cmd uimode.
    grep -F "original resource night: event=launch actual=$mode" \
        "$out/launch-$mode.log"
    grep -F "GLES original-raster renderer ready:" "$out/launch-$mode.log"
    grep -F "original dgulotta/kleinian raster ready:" "$out/launch-$mode.log"
    grep -F "original GPU frame probe" "$out/launch-$mode.log"

    adb exec-out screencap -p > "$out/display-$mode.png"
    test -s "$out/display-$mode.png"
    size="$(identify -format '%wx%h' "$out/display-$mode.png")"
    width="${size%x*}"
    height="${size#*x}"
    test "$width" -gt 100
    test "$height" -gt 100

    by_width=$((width / 6))
    by_height=$((height / 10))
    if [ "$by_width" -lt "$by_height" ]; then
        radius="$by_width"
    else
        radius="$by_height"
    fi
    # Inactive disk interior, away from crosshairs, rim, handle and anti-aliasing.
    x=$((width / 4 + radius * 45 / 100))
    y=$((135 * radius / 100 + radius * 35 / 100))
    read -r red green blue < <(
        convert "$out/display-$mode.png" \
            -crop "1x1+$x+$y" +repage -alpha off -depth 8 RGB:- |
            od -An -tu1
    )
    printf 'disk_rgb\t%s,%s,%s\n' "$red" "$green" "$blue" \
        > "$out/disk-$mode.tsv"
    for component in "$red" "$green" "$blue"; do
        # #2F2F2F is the inactive surface; allow small display rounding.
        test "$component" -ge 43
        test "$component" -le 51
    done

    # Check actual orientation/lifecycle response, not only a rotation request.
    adb logcat -c
    rotate_emulator 1
    sleep 4
    adb exec-out screencap -p > "$out/rotated-$mode.png"
    test -s "$out/rotated-$mode.png"
    rotated_size="$(identify -format '%wx%h' "$out/rotated-$mode.png")"
    rotated_width="${rotated_size%x*}"
    rotated_height="${rotated_size#*x}"
    test "$rotated_width" -eq "$height"
    test "$rotated_height" -eq "$width"
    adb logcat -d > "$out/rotated-$mode.log"
    grep -F "original GPU frame probe" "$out/rotated-$mode.log"
    rotate_emulator 0
    sleep 4

    # The already accepted independent decoded-RGB and trace gestures run
    # against this same signed APK in each *observed* system mode.
    bash tests/qemu-original-raster.sh "$apk" "$out/raster-$mode"
    bash tests/qemu-original-trace-controls.sh "$apk" "$out/traces-$mode"
    grep -F $'state\tPASS' "$out/raster-$mode/status.tsv"
    grep -F $'state\tPASS' "$out/traces-$mode/status.tsv"
    grep -F "original resource night: event=launch actual=$mode" \
        "$out/traces-$mode/before-trace-b-im.log"

    printf '%s\t%s\t%s\t%s\t%s,%s,%s\tPASS\tPASS\tPASS\n' \
        "$mode" "$actual_api" "$mode" "$mode" "$red" "$green" "$blue" \
        >> "$out/results.tsv"
}

check_mode no
check_mode yes

test "$(wc -l < "$out/results.tsv")" -eq 3
printf 'state\tPASS\napi\t%s\nmodes\tno,yes\n' "$actual_api" > "$out/status.tsv"
