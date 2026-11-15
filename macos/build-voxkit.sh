#!/bin/sh
# Build the existing fcitx5-voxkit daemon for this machine.
# On macOS the OpenCC lookup patch in macos/patches is applied first.
# Does not download model weights unless --download-models is passed.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
pin=9acbb7192733fee96ab787cf20f670349d05cefc
url=https://github.com/siygle/fcitx5-voxkit.git
src=${VOXKIT_SRC:-"$root/macos/build/fcitx5-voxkit"}
download=0
if [ "${1:-}" = "--download-models" ]; then
    download=1
fi

if ! command -v cargo >/dev/null 2>&1; then
    echo "cargo is required (Rust 1.85 or newer)" >&2
    exit 1
fi
if ! command -v git >/dev/null 2>&1; then
    echo "git is required" >&2
    exit 1
fi

if [ ! -d "$src/.git" ]; then
    git clone "$url" "$src"
fi
git -C "$src" fetch --depth 1 origin "$pin"
git -C "$src" checkout --detach "$pin"
if ! grep -q 'libopencc.1.dylib' "$src/daemon/voxkit/src/convert.rs"; then
    patch -d "$src" -p1 < "$root/macos/patches/voxkit-macos-opencc.patch"
fi

(
    cd "$src/daemon"
    cargo build --release
)
bin="$src/daemon/target/release/voxkit"
echo "voxkit binary: $bin"

if [ "$download" -eq 1 ]; then
    store=${VOXKIT_MODELS_DIR:-"$HOME/Library/Application Support/ChewingPlus/models"}
    if [ "$(uname)" != Darwin ]; then
        store=${VOXKIT_MODELS_DIR:-"$HOME/.local/share/voxkit/models"}
    fi
    mkdir -p "$store"
    VOXKIT_MODELS_DIR="$store" "$bin" model install qwen3-asr-0.6b
    echo "models: $store"
else
    echo "weights were not downloaded. On a Mac:"
    echo "  VOXKIT_MODELS_DIR=\"\$HOME/Library/Application Support/ChewingPlus/models\" \\"
    echo "    $bin model install qwen3-asr-0.6b"
fi
