#!/usr/bin/env bash
# Package each plugin into a version-named zip under dist/:
#
#   dist/<plugin>-<version>.zip
#     Win32/<plugin>.dll   32-bit DLL (for 32-bit CLaunch)
#     x64/<plugin>.dll     64-bit DLL (for 64-bit CLaunch)
#
# The version comes from <plugin>/version.h, so build.bat must run first.
# Usage: package.sh [plugin ...]   (default: every plugin directory)
set -euo pipefail

plugins="$*"
if [ -z "$plugins" ]; then
  plugins=$(for d in */; do
    p=${d%/}
    if [ -f "$p/$p.def" ]; then echo "$p"; fi
  done)
fi

mkdir -p dist
for p in $plugins; do
  ver=$(sed -n 's/^#define PLUGIN_VERSION_A[[:space:]]*"\([^"]*\)".*/\1/p' "$p/version.h")
  if [ -z "$ver" ]; then
    echo "[ERROR] $p/version.h has no PLUGIN_VERSION_A (run build.bat first)" >&2
    exit 1
  fi
  out="dist/$p-$ver.zip"
  # Compress-Archive flattens loose files, so stage the Win32/x64 layout
  # to keep the two DLLs (same file name) in separate folders inside the zip
  stage="dist/stage/$p"
  rm -rf "$stage"
  mkdir -p "$stage/Win32" "$stage/x64"
  cp "bin/Win32/$p.dll" "$stage/Win32/"
  cp "bin/x64/$p.dll" "$stage/x64/"
  rm -f "$out"
  powershell -NoProfile -Command "Compress-Archive -Path '$stage/*' -DestinationPath '$out' -Force"
  echo "[ZIP] $out"
done
