#!/bin/bash
set -e

echo "=== Test: qt.conf Loading ==="

QT_CONF="/opt/huntertransfer/qt.conf"
PLUGIN_DIR="/opt/huntertransfer/plugins"

if [ ! -f "$QT_CONF" ]; then
    echo "FAIL: qt.conf not found at $QT_CONF"
    exit 1
fi
echo "PASS: qt.conf exists"

if grep -q "Plugins = plugins" "$QT_CONF" && grep -q "Libraries = lib" "$QT_CONF"; then
    echo "PASS: qt.conf content correct"
else
    echo "FAIL: qt.conf content incorrect"
    cat "$QT_CONF"
    exit 1
fi

if [ -d "$PLUGIN_DIR/platforms" ]; then
    echo "PASS: Platform plugins directory exists"
    for plugin in libqxcb.so libqwayland-shellplugin.so; do
        if [ -f "$PLUGIN_DIR/platforms/$plugin" ]; then
            echo "PASS: $plugin found"
        else
            echo "WARN: $plugin not found"
        fi
    done
else
    echo "FAIL: Platform plugins directory not found"
    exit 1
fi

echo "=== qt.conf Loading Test PASSED ==="