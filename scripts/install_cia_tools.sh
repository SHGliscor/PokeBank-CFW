#!/usr/bin/env bash
set -euo pipefail

TOOLS_DIR="${DEVKITPRO:-/opt/devkitpro}/tools/bin"
mkdir -p "$TOOLS_DIR"

# Build the host tools from source inside the same Linux container that runs
# the 3DS build. This avoids incompatible prebuilt binaries / libc loaders.
if ! command -v bannertool >/dev/null 2>&1 || ! bannertool --help >/dev/null 2>&1; then
  echo "Building bannertool from source..."
  rm -rf /tmp/bannertool-src
  git clone --depth 1 https://github.com/Epicpkmn11/bannertool.git /tmp/bannertool-src
  make -C /tmp/bannertool-src -j2
  BANNERTOOL_BIN="$(find /tmp/bannertool-src -type f -name bannertool -perm -111 -print -quit)"
  test -n "$BANNERTOOL_BIN"
  install -m 0755 "$BANNERTOOL_BIN" "$TOOLS_DIR/bannertool"
fi

if ! command -v makerom >/dev/null 2>&1 || ! makerom -h >/dev/null 2>&1; then
  echo "Building makerom from source..."
  rm -rf /tmp/project-ctr
  git clone --depth 1 --branch makerom-v0.19.0 https://github.com/3DSGuy/Project_CTR.git /tmp/project-ctr
  make -C /tmp/project-ctr/makerom deps -j2
  make -C /tmp/project-ctr/makerom program -j2
  MAKEROM_BIN="$(find /tmp/project-ctr/makerom -type f -name makerom -perm -111 -print -quit)"
  test -n "$MAKEROM_BIN"
  install -m 0755 "$MAKEROM_BIN" "$TOOLS_DIR/makerom"
fi

export PATH="$TOOLS_DIR:$PATH"
bannertool --help >/dev/null 2>&1 || true
makerom -h >/dev/null 2>&1 || true
command -v bannertool
command -v makerom
