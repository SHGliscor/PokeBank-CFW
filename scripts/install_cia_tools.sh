#!/usr/bin/env bash
set -euo pipefail

TOOLS_DIR="${DEVKITPRO:-/opt/devkitpro}/tools/bin"
mkdir -p "$TOOLS_DIR"

# The bannertool release ZIP contains multiple platform binaries. Select the
# 64-bit Linux build explicitly; using the first `find` result can select the
# 32-bit binary, which cannot run in the devkitPro container.
if ! command -v bannertool >/dev/null 2>&1 || ! bannertool --help >/dev/null 2>&1; then
  echo "Installing bannertool v1.2.2 (linux-x86_64)..."
  rm -rf /tmp/bannertool /tmp/bannertool.zip
  wget -q "https://github.com/Epicpkmn11/bannertool/releases/download/v1.2.2/bannertool.zip" -O /tmp/bannertool.zip
  mkdir -p /tmp/bannertool
  unzip -q /tmp/bannertool.zip -d /tmp/bannertool
  BANNERTOOL_BIN="/tmp/bannertool/linux-x86_64/bannertool"
  test -x "$BANNERTOOL_BIN"
  install -m 0755 "$BANNERTOOL_BIN" "$TOOLS_DIR/bannertool"
fi

if ! command -v makerom >/dev/null 2>&1 || ! makerom -h >/dev/null 2>&1; then
  echo "Installing makerom v0.19.0 (linux-x86_64)..."
  rm -rf /tmp/makerom /tmp/makerom.zip
  wget -q "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-ubuntu_x86_64.zip" -O /tmp/makerom.zip
  echo "287b809dec064e0ad597e3d272c49ecb7eed41693d5ee6fef9d8a8aa24c2497e  /tmp/makerom.zip" | sha256sum -c -
  mkdir -p /tmp/makerom
  unzip -q /tmp/makerom.zip -d /tmp/makerom
  MAKEROM_BIN="$(find /tmp/makerom -type f -name makerom -perm -111 -print -quit)"
  test -n "$MAKEROM_BIN"
  install -m 0755 "$MAKEROM_BIN" "$TOOLS_DIR/makerom"
fi

export PATH="$TOOLS_DIR:$PATH"
command -v bannertool
command -v makerom
