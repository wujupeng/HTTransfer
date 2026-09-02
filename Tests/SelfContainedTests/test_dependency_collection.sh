#!/bin/bash
set -e

echo "=== Test: Dependency Collection ==="

EXECUTABLE="${1:-build-linux/App/HTTransfer}"
COLLECT_SCRIPT="${2:-debian/collect-deps.sh}"

if [ ! -f "$EXECUTABLE" ]; then
    echo "FAIL: Executable not found: $EXECUTABLE"
    exit 1
fi

TMPDIR=$(mktemp -d)
bash "$COLLECT_SCRIPT" "$EXECUTABLE" "$TMPDIR"

if [ ! -f "$TMPDIR/deps-list.txt" ]; then
    echo "FAIL: deps-list.txt not generated"
    exit 1
fi

DEP_COUNT=$(wc -l < "$TMPDIR/deps-list.txt")
echo "Collected ${DEP_COUNT} dependencies"

SYSTEM_LIBS="libc.so.6 libpthread.so.0 libdl.so.2 librt.so.1 libm.so.6 libgcc_s.so.1 libstdc++.so.6 ld-linux"
while IFS= read -r lib_path; do
    lib_name=$(basename "$lib_path")
    for sys_lib in $SYSTEM_LIBS; do
        if [ "$lib_name" = "$sys_lib" ]; then
            echo "FAIL: System library collected: $lib_name"
            exit 1
        fi
    done
done < "$TMPDIR/deps-list.txt"
echo "PASS: No system libraries in collection"

SORTED=$(sort "$TMPDIR/deps-list.txt")
UNIQ=$(sort -u "$TMPDIR/deps-list.txt")
if [ "$SORTED" = "$UNIQ" ]; then
    echo "PASS: No duplicate entries"
else
    echo "FAIL: Duplicate entries found"
    exit 1
fi

QT_FOUND=false
while IFS= read -r lib_path; do
    lib_name=$(basename "$lib_path")
    case "$lib_name" in
        libQt6Core.so.6*) QT_FOUND=true ;;
    esac
done < "$TMPDIR/deps-list.txt"

if [ "$QT_FOUND" = true ]; then
    echo "PASS: Qt6 dependencies collected"
else
    echo "FAIL: Qt6 dependencies not found"
    exit 1
fi

rm -rf "$TMPDIR"
echo "=== Dependency Collection Test PASSED ==="