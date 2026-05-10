#!/usr/bin/env bash
#
# run.sh — integration test for the brscan end-of-page hang.
#
# Builds the LD_PRELOAD libusb stub and the SANE-API harness, then drives
# libsane-brother.so against the captured DCP-1510 transcript. The brscan
# parser is expected to hang at end of page (currently observed bug);
# the harness detects this and exits with code 2 → test PASSES on bug-
# reproduced. Once the parser is fixed the hang detector will not trigger
# and the harness will exit 0; this script then reports FAIL meaning
# "bug no longer reproduces — flip the assertion".
#
# Usage:
#   tests/eop_hang/run.sh [path/to/libsane-brother.so.1]
#
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
LIB="${1:-$ROOT/build/libsane-brother.so.1}"
DATA="$ROOT/tests/data/dcp1510_a4_truegray_200dpi.bin"

if [[ ! -f "$LIB" ]]; then
    echo "FAIL: library not found at $LIB (build first: cmake --build $ROOT/build)"
    exit 1
fi
if [[ ! -f "$DATA" ]]; then
    echo "FAIL: capture not found at $DATA"
    exit 1
fi

BUILD="$HERE/build"
mkdir -p "$BUILD"

echo "[run] building usb_stub.so + harness ..."
cc -shared -fPIC -O2 -Wall -Wno-unused-parameter \
    -o "$BUILD/usb_stub.so" "$HERE/usb_stub.c"
cc -O0 -g -Wall -Wno-unused-parameter \
    -o "$BUILD/harness" "$HERE/harness.c" -ldl

echo "[run] launching harness with replay='$DATA'"
set +e
LD_PRELOAD="$BUILD/usb_stub.so" \
    LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}:/usr/local/lib" \
    BRSCAN_REPLAY_FILE="$DATA" \
    "$BUILD/harness" "$LIB" "bus1;dev1"
RC=$?
set -e

echo "[run] harness exit code: $RC"
case "$RC" in
    0)  echo "FAIL: scan completed cleanly with full image — end-of-page hang"
        echo "      no longer reproduces. Once you intentionally fix the bug,"
        echo "      flip this script to expect RC=0 as PASS."
        exit 1 ;;
    2)  echo "PASS: end-of-page hang reproduced (truncated / spin / timeout)."
        exit 0 ;;
    *)  echo "ERROR: harness exited with $RC (setup failure or unexpected status)."
        exit "$RC" ;;
esac
