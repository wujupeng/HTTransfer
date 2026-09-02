#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-linux"
PKG_DIR="${PROJECT}/dist-linux"
VERSION="0.1.0-alpha.9"
ARCH="amd64"
PKG_NAME="httransfer_${VERSION}_${ARCH}"
PKG_FULL="${PKG_DIR}/${PKG_NAME}"

echo "=== Building HTTransfer for Linux ==="
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake "${PROJECT_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr

ninja

echo "=== Creating .deb package ==="
mkdir -p "${PKG_FULL}/usr/bin"
mkdir -p "${PKG_FULL}/usr/share/applications"
mkdir -p "${PKG_FULL}/usr/share/icons/hicolor/256x256/apps"
mkdir -p "${PKG_FULL}/DEBIAN"

cp "${BUILD_DIR}/App/HTTransfer" "${PKG_FULL}/usr/bin/HTTransfer"
chmod 755 "${PKG_FULL}/usr/bin/HTTransfer"

cp "${PROJECT_DIR}/debian/httransfer.desktop" "${PKG_FULL}/usr/share/applications/"

if [ -f "${PROJECT_DIR}/Resources/app.png" ]; then
    cp "${PROJECT_DIR}/Resources/app.png" "${PKG_FULL}/usr/share/icons/hicolor/256x256/apps/httransfer.png"
fi

cat > "${PKG_FULL}/DEBIAN/control" << EOF
Package: httransfer
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: HTTransfer Team <wujupeng@users.noreply.github.com>
Depends: qt6-base-dev (>= 6.4), libssl3 (>= 3.0), libsqlite3-0, libzstd1, libblake3-0
Section: utils
Priority: optional
Description: High-Performance Local File Copy Engine
 HTTransfer is a world-class local file copy engine, similar to
 FastCopy / TeraCopy / Robocopy (GUI version).
EOF

cp "${PROJECT_DIR}/debian/postinst" "${PKG_FULL}/DEBIAN/postinst"
chmod 755 "${PKG_FULL}/DEBIAN/postinst"

cp "${PROJECT_DIR}/debian/postrm" "${PKG_FULL}/DEBIAN/postrm"
chmod 755 "${PKG_FULL}/DEBIAN/postrm"

dpkg-deb --build "${PKG_FULL}"

echo "=== Package created: ${PKG_FULL}.deb ==="
ls -lh "${PKG_FULL}.deb"