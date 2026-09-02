#!/bin/bash
set -e

echo "=== Test: Offline Installation ==="

DEB_FILE="${1:-dist-linux/httransfer_1.0.0-alpha.10_amd64.deb}"

if [ ! -f "$DEB_FILE" ]; then
    echo "FAIL: .deb file not found: $DEB_FILE"
    exit 1
fi

echo "Step 1: Check current dependency state"
dpkg -l httransfer 2>/dev/null && {
    echo "WARN: httransfer already installed, removing first..."
    dpkg -r httransfer 2>/dev/null || true
}

echo "Step 2: Install with dpkg -i (no apt, no network)"
dpkg -i "$DEB_FILE"
INSTALL_RC=$?

if [ $INSTALL_RC -ne 0 ]; then
    echo "FAIL: dpkg -i failed with exit code $INSTALL_RC"
    exit 1
fi
echo "PASS: dpkg -i succeeded"

echo "Step 3: Verify installation"
if command -v HTTransfer >/dev/null 2>&1; then
    echo "PASS: HTTransfer in PATH"
else
    echo "FAIL: HTTransfer not in PATH"
    exit 1
fi

if [ -d "/opt/huntertransfer/lib" ]; then
    echo "PASS: Private lib directory exists"
else
    echo "FAIL: Private lib directory missing"
    exit 1
fi

if [ -f "/opt/huntertransfer/qt.conf" ]; then
    echo "PASS: qt.conf exists"
else
    echo "FAIL: qt.conf missing"
    exit 1
fi

if [ -L "/usr/bin/HTTransfer" ]; then
    echo "PASS: Symlink exists"
else
    echo "FAIL: Symlink missing"
    exit 1
fi

echo "Step 4: Verify version"
HTTransfer --version

echo "=== Offline Installation Test PASSED ==="