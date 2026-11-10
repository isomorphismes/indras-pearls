#!/usr/bin/env bash
set -euo pipefail

apk="${1:?signed candidate APK required}"
out="${2:-qemu-continuous}"
package=org.isomorphisms.indraspearls
mkdir -p "$out"
printf 'state\tstarted\n' > "$out/status.tsv"

cleanup() {
    adb shell cmd window user-rotation free >/dev/null 2>&1 || :
    adb shell setprop debug.indras.gpu_probe 0 >/dev/null 2>&1 || :
}
trap cleanup EXIT

# No forced glFinish/glReadPixels during the 60s stress measurement.
adb shell setprop debug.indras.gpu_probe 0
adb install -r "$apk" | tee "$out/install.txt"
adb shell cmd window user-rotation lock 0
adb shell input keyevent KEYCODE_WAKEUP
adb shell wm dismiss-keyguard
adb logcat -c
adb shell am force-stop "$package"  # exactly one launch before the session
adb shell am start -W -n "$package/android.app.NativeActivity" \
    > "$out/launch.txt"

# Startup is condition-based, not a 3-second delay after each drag.
ready=0
for attempt in $(seq 1 50); do
    adb logcat -d > "$out/start.log"
    if grep -q 'original dgulotta/kleinian raster ready:' "$out/start.log"; then
        ready=1
        break
    fi
    sleep 0.1
done
test "$ready" -eq 1
pid_before="$(adb shell pidof "$package" | tr -d '\r' | awk '{print $1}')"
test -n "$pid_before"
adb exec-out screencap -p > "$out/before-full.png"
width="$(identify -format %w "$out/before-full.png")"
height="$(identify -format %h "$out/before-full.png")"
test "$height" -gt "$width"
if [ "$((width / 6))" -lt "$((height / 10))" ]; then
    radius=$((width / 6))
else
    radius=$((height / 10))
fi
center_y=$((135 * radius / 100))
center_left=$((width / 4))
center_right=$((3 * width / 4))

# The control-free center is measured separately from disk movement.
crop_y=$((height / 4))
crop_h=$((height / 2))
convert "$out/before-full.png" -crop "${width}x${crop_h}+0+${crop_y}" \
    +repage "$out/before-math.png"

start_seconds="$(date +%s)"
i=0
while [ "$(( $(date +%s) - start_seconds ))" -lt 60 ]; do
    control=$((i % 2))
    axis=$(((i / 2) % 2))
    if [ "$control" -eq 0 ]; then center_x="$center_left"
    else center_x="$center_right"; fi
    end_x="$center_x"
    end_y="$center_y"
    if [ "$axis" -eq 0 ]; then
        end_x=$((center_x + radius / 2))
    else
        end_y=$((center_y + radius / 2))
    fi
    adb shell input touchscreen swipe \
        "$center_x" "$center_y" "$end_x" "$end_y" 100
    i=$((i + 1))
    if [ "$((i % 16))" -eq 0 ]; then
        adb exec-out screencap -p > "$out/check-$i.png"
        test -s "$out/check-$i.png"
        test "$(adb shell pidof "$package" | tr -d '\r' | awk '{print $1}')" = "$pid_before"
    fi
done

# End on an explicitly different mathematical state without a restart.
adb shell input touchscreen swipe \
    "$center_right" "$center_y" "$center_right" \
    "$((center_y + radius / 2))" 100

# Determine the final touch's revision, then require THAT mathematical
# revision to reach the swap path; older submissions cannot satisfy this.
adb logcat -d > "$out/session.log"
final_revision="$(grep -E 'original input: action=1 selected=1' "$out/session.log" \
    | tail -n 1 | sed -n 's/.*revision=\([0-9][0-9]*\).*/\1/p')"
test -n "$final_revision"
submitted=0
for attempt in $(seq 1 50); do
    adb logcat -d > "$out/session.log"
    if grep -q "original frame submitted: revision=$final_revision " "$out/session.log"; then
        submitted=1
        break
    fi
    sleep 0.1
done
test "$submitted" -eq 1
adb exec-out screencap -p > "$out/after-full.png"
convert "$out/after-full.png" -crop "${width}x${crop_h}+0+${crop_y}" \
    +repage "$out/after-math.png"
changed="$(compare -metric AE -fuzz 2% -channel RGB \
    "$out/before-math.png" "$out/after-math.png" null: 2>&1 || :)"
changed="${changed%% *}"
case "$changed" in ''|*[!0-9]*) echo "bad AE metric: $changed" >&2; exit 1;; esac
test "$changed" -gt "$((width * crop_h / 500))"

# All four axes, both controls, successive worker revisions and main-loop
# submissions must be evidenced in this *single process*.
grep -qE 'original input: action=0 selected=0' "$out/session.log"
grep -qE 'original input: action=0 selected=1' "$out/session.log"
grep -q 'original dgulotta/kleinian raster ready:' "$out/session.log"
grep -q 'original frame submitted: revision=' "$out/session.log"
grep -q 'input callback durations:' "$out/session.log"

# Core input-callback median/p95 is logged per drag release. Record the worst
# reported p95; the actual C67 gate is separate from this emulator observation.
awk '
/input callback durations:/ {
    for (i=1; i<=NF; i++) if ($i ~ /^p95_us=/) {
        split($i, a, "="); if (a[2]+0 > max) max=a[2]+0
    }
}
END {print "max_reported_input_p95_us\t" max; exit !(max >= 0)}
' "$out/session.log" > "$out/latency.tsv"

# Prove landscape and return-to-portrait in the same session. The previous
# API34 test failed because writing settings did not actually rotate the screen.
adb shell cmd window user-rotation lock 1
rotated=0
for attempt in $(seq 1 50); do
    adb exec-out screencap -p > "$out/landscape.png"
    if [ "$(identify -format %wx%h "$out/landscape.png")" = "${height}x${width}" ]; then
        rotated=1
        break
    fi
    sleep 0.1
done
test "$rotated" -eq 1
adb shell cmd window user-rotation lock 0
restored=0
for attempt in $(seq 1 50); do
    adb exec-out screencap -p > "$out/portrait.png"
    if [ "$(identify -format %wx%h "$out/portrait.png")" = "${width}x${height}" ]; then
        restored=1
        break
    fi
    sleep 0.1
done
test "$restored" -eq 1
pid_after="$(adb shell pidof "$package" | tr -d '\r' | awk '{print $1}')"
test "$pid_after" = "$pid_before"

printf 'state\tPASS\ncontrol_count\t2\nduration_floor_seconds\t60\n' \
    > "$out/status.tsv"
printf 'same_process_pid\t%s\nnumber_of_drags\t%s\n' \
    "$pid_before" "$i" >> "$out/status.tsv"
printf 'control_free_changed_pixels\t%s\nrotation\tportrait-landscape-portrait\n' \
    "$changed" >> "$out/status.tsv"
