#!/bin/sh
# press-test: watch the controller at the raw HID layer and through the Game Controller
# framework at the same time. Press controls, then stop with Ctrl-C to get a summary.
# Usage: tools/press-test.sh [label]
cd "$(dirname "$0")/.."
mkdir -p build notes/captures
LABEL=${1:-presses}
STAMP=$(date +%Y-%m-%d-%H%M)
HID=notes/captures/$STAMP-hid-$LABEL.txt
GC=notes/captures/$STAMP-gc-$LABEL.txt

[ -x build/hid_watch ] || clang -o build/hid_watch tools/hid_watch.c -framework IOKit -framework CoreFoundation
[ -x build/gc_watch ] || swiftc -O tools/gc_watch.swift -o build/gc_watch

build/hid_watch 3600 > "$HID" 2>&1 &
HIDPID=$!
build/gc_watch 3600 > "$GC" 2>&1 &
GCPID=$!
# On Ctrl-C: stop the watchers, then fall through to the summary below.
trap 'kill $HIDPID $GCPID 2>/dev/null' INT TERM

echo "Watching. Press the controls you want to test, then press Ctrl-C."
wait $HIDPID 2>/dev/null
wait $GCPID 2>/dev/null
trap - INT TERM

echo
echo "=== Raw HID elements (usage page/usage: changes)"
grep -E '^page' "$HID" | awk '{n[$2" "$4]++} END {for (k in n) printf "  %s: %d\n", k, n[k]}' | sort
echo
echo "=== Game Controller framework elements (changes)"
grep -E '^  (button|dpad|axis)' "$GC" | sed -E 's/ = .*//' | awk '{$1=$1; n[$0]++} END {for (k in n) printf "  %s: %d\n", k, n[k]}' | sort
echo
echo "=== Presses in order (first time each Game Controller button went to 1.0)"
grep -nE 'button .* = 1.0' "$GC" | sed -E 's/ = 1.0//; s/^([0-9]+):\s*button /\1: /' | awk -F': ' '!seen[$2]++' | head -20
echo
echo "Logs: $HID and $GC"
