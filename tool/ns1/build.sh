#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
export DEVKITPRO="${DEVKITPRO:-$ROOT/tool/devkitpro}"
export CARGO_HOME="$ROOT/tool/cache/cargo"
export RUSTUP_HOME="$ROOT/tool/cache/rustup"
export TMPDIR="$ROOT/build/tmp"
export PATH="$CARGO_HOME/bin:$PATH"
mkdir -p "$TMPDIR" "$CARGO_HOME" "$RUSTUP_HOME"
test -f "$DEVKITPRO/cmake/Switch.cmake" || { echo "Missing devkitPro/libnx. See docs/NS1-Tailscale.zh-CN.md" >&2; exit 1; }
command -v cmake >/dev/null
command -v cargo >/dev/null
command -v node >/dev/null
cargo +nightly-2026-09-25 --version
cmake -S "$ROOT" -B "$ROOT/build/ns1" -DCMAKE_BUILD_TYPE=Release \
  -DPLATFORM_SWITCH=ON -DPLATFORM_SWITCH2=OFF -DUSE_DEKO3D=ON -DENABLE_EMBEDDED_TAILSCALE=ON
cmake --build "$ROOT/build/ns1" --target Moonlight.nro --parallel "${JOBS:-4}"
STAGING="$ROOT/build/ns1/sdcard/switch/Moonlight-T-NX"
mkdir -p "$STAGING" "$ROOT/dist"
cp "$ROOT/build/ns1/Moonlight.nro" "$STAGING/Moonlight-T-NX.nro"
cp "$ROOT/build/ns1/Moonlight.nro" "$ROOT/dist/Moonlight-T-NX.nro"
cp "$ROOT/tool/ns1/tailscale.example.ini" "$ROOT/docs/NS1-Tailscale.zh-CN.md" "$STAGING/"
(cd "$ROOT/build/ns1/sdcard" && cmake -E tar cf "$ROOT/dist/Moonlight-T-NX-NS1.zip" --format=zip switch)
(cd "$ROOT" && node tool/ns1/verify-nro.mjs)
echo "Created $ROOT/dist/Moonlight-T-NX-NS1.zip"
