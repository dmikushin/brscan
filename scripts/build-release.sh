#!/usr/bin/env bash
# Build a release tarball inside a target-arch container.
#
# Required environment:
#   VERSION  — release version string (e.g. v1.0.0)
#   ARCH     — short arch name (amd64 / arm64 / armv7)
#
# Run inside the container with the source tree mounted at /work.
set -euo pipefail

: "${VERSION:?VERSION must be set}"
: "${ARCH:?ARCH must be set}"

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config \
    libsane-dev libusb-dev libusb-1.0-0-dev libjpeg-dev libncurses-dev \
    ca-certificates

cd /work
rm -rf build
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j"$(nproc)"

cd /work
STAGE="brscan-${VERSION}-${ARCH}"
DEST="dist/${STAGE}"
rm -rf "${DEST}"
mkdir -p "${DEST}/lib/sane" "${DEST}/lib" "${DEST}/bin"

install -m 644 build/libsane-brother.so.1 "${DEST}/lib/sane/"
install -m 644 build/libbrscandec.so.1     "${DEST}/lib/"
install -m 644 build/libbrcolm.so.1        "${DEST}/lib/"
install -m 755 build/brsaneconfig          "${DEST}/bin/"

for f in README.md Copying copying.brother copying.lib; do
    [ -f "$f" ] && cp "$f" "${DEST}/"
done

# Plain text install instructions — no heredoc indent traps.
{
    echo 'Install (run as root):'
    echo
    echo '    install -m 644 lib/libbrscandec.so.1     /usr/lib/libbrscandec.so.1'
    echo '    install -m 644 lib/libbrcolm.so.1        /usr/lib/libbrcolm.so.1'
    echo '    install -m 644 lib/sane/libsane-brother.so.1 /usr/lib/sane/libsane-brother.so.1'
    echo '    ln -sf libbrscandec.so.1 /usr/lib/libbrscandec.so'
    echo '    ln -sf libbrscandec.so.1 /usr/lib/libbrscandec2.so'
    echo '    ln -sf libbrcolm.so.1    /usr/lib/libbrcolm.so'
    echo '    ln -sf libbrcolm.so.1    /usr/lib/libbrcolm2.so'
    echo '    ln -sf libsane-brother.so.1 /usr/lib/sane/libsane-brother.so'
    echo '    ldconfig'
    echo '    echo brother >> /etc/sane.d/dll.conf'
    echo
    echo 'Then add a udev rule (see README.md for details).'
} > "${DEST}/INSTALL"

cd dist
tar czf "${STAGE}.tar.gz" "${STAGE}"
sha256sum "${STAGE}.tar.gz" > "${STAGE}.tar.gz.sha256"
ls -la

# Files were created by root (container default user); chmod so the
# GitHub Actions runner can upload them as artifacts.
chmod -R a+rwX .
if [ -n "${HOST_UID:-}" ]; then
    chown -R "${HOST_UID}:${HOST_GID:-${HOST_UID}}" .
fi
