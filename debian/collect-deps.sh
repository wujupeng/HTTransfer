#!/bin/bash
set -e

SCRIPT_NAME="collect-deps.sh"

if [ $# -lt 2 ]; then
    echo "Usage: $SCRIPT_NAME <executable> <output_dir>" >&2
    exit 1
fi

EXECUTABLE="$1"
OUTPUT_DIR="$2"

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable not found: $EXECUTABLE" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

SYSTEM_LIB_WHITELIST=(
    "libc.so.6"
    "libpthread.so.0"
    "libdl.so.2"
    "librt.so.1"
    "libm.so.6"
    "libgcc_s.so.1"
    "libstdc++.so.6"
    "ld-linux-x86-64.so.2"
    "libglibc.so"
    "libresolv.so.2"
    "libanl.so.1"
    "libcrypt.so.1"
    "libutil.so.1"
    "libBrokenLocale.so.1"
    "libnsl.so.1"
    "libnss_compat.so.2"
    "libnss_dns.so.2"
    "libnss_files.so.2"
    "libnss_hesiod.so.2"
    "libnss_nis.so.2"
    "libnss_nisplus.so.2"
)

is_system_lib() {
    local lib_path="$1"
    local lib_name=$(basename "$lib_path")
    for sys_lib in "${SYSTEM_LIB_WHITELIST[@]}"; do
        if [ "$lib_name" = "$sys_lib" ]; then
            return 0
        fi
    done
    return 1
}

declare -A COLLECTED_LIBS

collect_deps_recursive() {
    local target="$1"
    local ldd_output

    if ! ldd_output=$(ldd "$target" 2>/dev/null); then
        return 0
    fi

    while IFS= read -r line; do
        local lib_path
        if echo "$line" | grep -q "=>"; then
            lib_path=$(echo "$line" | sed 's/.*=> \(.*\) (.*/\1/' | xargs)
        else
            lib_path=$(echo "$line" | awk '{print $1}' | xargs)
        fi

        if [ -z "$lib_path" ] || [ "$lib_path" = "not" ] || [ ! -f "$lib_path" ]; then
            continue
        fi

        if is_system_lib "$lib_path"; then
            continue
        fi

        if [ -z "${COLLECTED_LIBS[$lib_path]}" ]; then
            COLLECTED_LIBS[$lib_path]=1
            collect_deps_recursive "$lib_path"
        fi
    done <<< "$ldd_output"
}

echo "=== Collecting dependencies for: $EXECUTABLE ===" >&2
collect_deps_recursive "$EXECUTABLE"

OUTPUT_FILE="$OUTPUT_DIR/deps-list.txt"
: > "$OUTPUT_FILE"

for lib_path in "${!COLLECTED_LIBS[@]}"; do
    echo "$lib_path" >> "$OUTPUT_FILE"
done

sort -u "$OUTPUT_FILE" -o "$OUTPUT_FILE"

local_count=$(wc -l < "$OUTPUT_FILE")
echo "=== Collected $local_count dependencies (system libs filtered) ===" >&2
echo "=== Output: $OUTPUT_FILE ===" >&2