#!/bin/bash
set -e

echo "=== Test: RPATH Embedding ==="

EXECUTABLE="${1:-build-linux/App/HTTransfer}"

if [ ! -f "$EXECUTABLE" ]; then
    echo "FAIL: Executable not found: $EXECUTABLE"
    exit 1
fi

RUNPATH=$(readelf -d "$EXECUTABLE" | grep RUNPATH | sed 's/.*\[\(.*\)\]/\1/')
if [ "$RUNPATH" = "\$ORIGIN/../lib" ]; then
    echo "PASS: DT_RUNPATH = \$ORIGIN/../lib"
else
    echo "FAIL: DT_RUNPATH = '$RUNPATH' (expected '\$ORIGIN/../lib')"
    exit 1
fi

if readelf -d "$EXECUTABLE" | grep -q "RPATH"; then
    echo "FAIL: DT_RPATH present (should use DT_RUNPATH only)"
    exit 1
else
    echo "PASS: No DT_RPATH (using new dtags)"
fi

echo "=== RPATH Embedding Test PASSED ==="