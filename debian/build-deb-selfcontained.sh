#!/bin/bash
set -e

SCRIPT_NAME="build-deb-selfcontained.sh"
VERSION=""
ARCH="amd64"
MAX_SIZE_MB=200
SKIP_STRIP=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            VERSION="$2"; shift 2 ;;
        --arch)
            ARCH="$2"; shift 2 ;;
        --max-size)
            MAX_SIZE_MB="$2"; shift 2 ;;
        --skip-strip)
            SKIP_STRIP=true; shift ;;
        *)
            echo "Usage: $SCRIPT_NAME [--version <version>] [--arch <arch>] [--max-size <mb>] [--skip-strip]" >&2
            exit 1 ;;
    esac
done

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-linux"
DIST_DIR="${PROJECT_DIR}/dist-linux"
PRIVATE_ROOT="/opt/huntertransfer"
PRIVATE_LIB="${PRIVATE_ROOT}/lib"
PRIVATE_BIN="${PRIVATE_ROOT}/bin"
PRIVATE_PLUGIN="${PRIVATE_ROOT}/plugins"

if [ -z "$VERSION" ]; then
    VERSION=$(grep -oP 'set\(HT_VERSION_PRERELEASE "\K[^"]+' "${PROJECT_DIR}/CMakeLists.txt" 2>/dev/null || echo "alpha.10")
    VERSION="1.0.0-${VERSION}"
fi

PKG_NAME="httransfer_${VERSION}_${ARCH}"
PKG_DIR="${BUILD_DIR}/pkg-${PKG_NAME}"
DEB_FILE="${DIST_DIR}/${PKG_NAME}.deb"

echo "=== Building HTTransfer Self-Contained Package ==="
echo "  Version: ${VERSION}"
echo "  Arch:    ${ARCH}"
echo "  MaxSize: ${MAX_SIZE_MB}MB"
echo "  SkipStrip: ${SKIP_STRIP}"
echo ""

echo "=== Phase 1: CMake Configure + Build ==="
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake "${PROJECT_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PRIVATE_ROOT}"

if ! ninja; then
    echo "ERROR: Build failed" >&2
    exit 1
fi

echo "=== Phase 2: Prepare Package Directory ==="
rm -rf "${PKG_DIR}"
mkdir -p "${PKG_DIR}${PRIVATE_BIN}"
mkdir -p "${PKG_DIR}${PRIVATE_LIB}"
mkdir -p "${PKG_DIR}${PRIVATE_PLUGIN}"
mkdir -p "${PKG_DIR}/usr/bin"
mkdir -p "${PKG_DIR}/usr/share/applications"
mkdir -p "${PKG_DIR}/usr/share/icons/hicolor/256x256/apps"
mkdir -p "${PKG_DIR}/DEBIAN"

echo "=== Phase 3: Install Executable ==="
cp "${BUILD_DIR}/App/HTTransfer" "${PKG_DIR}${PRIVATE_BIN}/HTTransfer"
chmod 755 "${PKG_DIR}${PRIVATE_BIN}/HTTransfer"

ln -sf "${PRIVATE_BIN}/HTTransfer" "${PKG_DIR}/usr/bin/HTTransfer"

echo "=== Phase 4: Collect Dependencies ==="
DEPS_TMP=$(mktemp -d)
bash "${PROJECT_DIR}/debian/collect-deps.sh" "${BUILD_DIR}/App/HTTransfer" "${DEPS_TMP}"

if [ ! -f "${DEPS_TMP}/deps-list.txt" ]; then
    echo "ERROR: Dependency collection failed" >&2
    exit 2
fi

DEP_COUNT=0
while IFS= read -r lib_path; do
    if [ -f "$lib_path" ]; then
        cp -L "$lib_path" "${PKG_DIR}${PRIVATE_LIB}/"
        DEP_COUNT=$((DEP_COUNT + 1))
    fi
done < "${DEPS_TMP}/deps-list.txt"
echo "  Collected ${DEP_COUNT} runtime libraries"

echo "=== Phase 5: Collect Qt Plugins ==="
QT_PLUGIN_DIR=$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || echo "")

if [ -z "$QT_PLUGIN_DIR" ] || [ ! -d "$QT_PLUGIN_DIR" ]; then
    echo "ERROR: Qt plugin directory not found" >&2
    exit 3
fi

for plugin_subdir in platforms platformthemes imageformats iconengines styles; do
    if [ -d "${QT_PLUGIN_DIR}/${plugin_subdir}" ]; then
        mkdir -p "${PKG_DIR}${PRIVATE_PLUGIN}/${plugin_subdir}"
        for so_file in "${QT_PLUGIN_DIR}/${plugin_subdir}"/*.so; do
            if [ -f "$so_file" ]; then
                base_name=$(basename "$so_file")
                case "$base_name" in
                    libqwayland-shellplugin.so|libqwayland-egl.so|libqxcb.so|libqoffscreen.so|libqminimal.so|libqvnc.so|libqlinuxfb.so)
                        cp "$so_file" "${PKG_DIR}${PRIVATE_PLUGIN}/${plugin_subdir}/"
                        ;;
                    libqsvg.so|libqsvgicon.so|libqgtk3.so|libqxdgdesktopportal.so)
                        cp "$so_file" "${PKG_DIR}${PRIVATE_PLUGIN}/${plugin_subdir}/"
                        ;;
                esac
            fi
        done
    fi
done

echo "  Qt plugins collected"

echo "=== Phase 6: Collect Qt Plugin Dependencies ==="
for so_file in $(find "${PKG_DIR}${PRIVATE_PLUGIN}" -name "*.so" -type f); do
    bash "${PROJECT_DIR}/debian/collect-deps.sh" "$so_file" "${DEPS_TMP}" 2>/dev/null || true
    if [ -f "${DEPS_TMP}/deps-list.txt" ]; then
        while IFS= read -r lib_path; do
            if [ -f "$lib_path" ] && [ ! -f "${PKG_DIR}${PRIVATE_LIB}/$(basename $lib_path)" ]; then
                cp -L "$lib_path" "${PKG_DIR}${PRIVATE_LIB}/" 2>/dev/null || true
            fi
        done < "${DEPS_TMP}/deps-list.txt"
    fi
done

echo "=== Phase 7: Install qt.conf ==="
cp "${PROJECT_DIR}/debian/qt.conf.template" "${PKG_DIR}${PRIVATE_ROOT}/qt.conf"

echo "=== Phase 8: Desktop Integration ==="
cp "${PROJECT_DIR}/debian/httransfer.desktop" "${PKG_DIR}/usr/share/applications/"
chmod 644 "${PKG_DIR}/usr/share/applications/httransfer.desktop"

if [ -f "${PROJECT_DIR}/Resources/app.png" ]; then
    cp "${PROJECT_DIR}/Resources/app.png" "${PKG_DIR}/usr/share/icons/hicolor/256x256/apps/httransfer.png"
fi

echo "=== Phase 9: DEBIAN Control Files ==="
cp "${PROJECT_DIR}/debian/control-selfcontained" "${PKG_DIR}/DEBIAN/control"
sed -i "s/Version: .*/Version: ${VERSION}/" "${PKG_DIR}/DEBIAN/control"
sed -i "s/Architecture: .*/Architecture: ${ARCH}/" "${PKG_DIR}/DEBIAN/control"

cp "${PROJECT_DIR}/debian/postinst" "${PKG_DIR}/DEBIAN/postinst"
chmod 755 "${PKG_DIR}/DEBIAN/postinst"

cp "${PROJECT_DIR}/debian/postrm" "${PKG_DIR}/DEBIAN/postrm"
chmod 755 "${PKG_DIR}/DEBIAN/postrm"

echo "=== Phase 10: Strip Debug Symbols ==="
if [ "$SKIP_STRIP" = false ]; then
    strip "${PKG_DIR}${PRIVATE_BIN}/HTTransfer" 2>/dev/null || true
    find "${PKG_DIR}${PRIVATE_LIB}" -name "*.so*" -exec strip {} \; 2>/dev/null || true
    find "${PKG_DIR}${PRIVATE_PLUGIN}" -name "*.so" -exec strip {} \; 2>/dev/null || true
    echo "  Debug symbols stripped"
else
    echo "  Skipping strip (as requested)"
fi

echo "=== Phase 11: Build .deb Package ==="
mkdir -p "${DIST_DIR}"

if ! dpkg-deb --build "${PKG_DIR}" "${DEB_FILE}"; then
    echo "ERROR: dpkg-deb build failed" >&2
    exit 4
fi

echo "=== Phase 12: Package Size Check ==="
PKG_SIZE_BYTES=$(stat -c%s "${DEB_FILE}")
PKG_SIZE_MB=$((PKG_SIZE_BYTES / 1024 / 1024))

echo ""
echo "=== Build Complete ==="
echo "  Package: ${DEB_FILE}"
echo "  Size:    ${PKG_SIZE_MB}MB (${PKG_SIZE_BYTES} bytes)"

if [ ${PKG_SIZE_MB} -gt ${MAX_SIZE_MB} ]; then
    echo "  WARNING: Package size ${PKG_SIZE_MB}MB exceeds limit ${MAX_SIZE_MB}MB" >&2
fi

echo ""
echo "  Install with: sudo dpkg -i ${DEB_FILE}"

rm -rf "${DEPS_TMP}"
echo "=== Done ==="