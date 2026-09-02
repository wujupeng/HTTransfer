#!/bin/bash
set -e

echo "=== Test: Package Size ==="

DEB_FILE="${1:-dist-linux/httransfer_1.0.0-alpha.10_amd64.deb}"
MAX_SIZE_MB=${2:-200}

if [ ! -f "$DEB_FILE" ]; then
    echo "FAIL: .deb file not found: $DEB_FILE"
    exit 1
fi

PKG_SIZE_BYTES=$(stat -c%s "$DEB_FILE")
PKG_SIZE_MB=$((PKG_SIZE_BYTES / 1024 / 1024))

echo "Package: $DEB_FILE"
echo "Size: ${PKG_SIZE_MB}MB (${PKG_SIZE_BYTES} bytes)"
echo "Limit: ${MAX_SIZE_MB}MB"

if [ ${PKG_SIZE_MB} -gt ${MAX_SIZE_MB} ]; then
    echo "FAIL: Package size ${PKG_SIZE_MB}MB exceeds limit ${MAX_SIZE_MB}MB"
    echo "Consider removing unused Qt6 modules to reduce size"
    exit 1
else
    echo "PASS: Package size within limit"
fi

echo ""
echo "Size breakdown:"
if command -v dpkg-deb >/dev/null 2>&1; then
    TMPDIR=$(mktemp -d)
    dpkg-deb -x "$DEB_FILE" "$TMPDIR"
    
    BIN_SIZE=$(du -sm "$TMPDIR/opt/huntertransfer/bin" 2>/dev/null | awk '{print $1}')
    LIB_SIZE=$(du -sm "$TMPDIR/opt/huntertransfer/lib" 2>/dev/null | awk '{print $1}')
    PLUGIN_SIZE=$(du -sm "$TMPDIR/opt/huntertransfer/plugins" 2>/dev/null | awk '{print $1}')
    
    echo "  Binary:     ${BIN_SIZE:-0}MB"
    echo "  Libraries:  ${LIB_SIZE:-0}MB"
    echo "  Plugins:    ${PLUGIN_SIZE:-0}MB"
    
    rm -rf "$TMPDIR"
fi

echo "=== Package Size Test PASSED ==="