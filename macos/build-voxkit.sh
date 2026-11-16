#!/bin/sh
# Build the existing fcitx5-voxkit daemon for this machine.
# The pin is shared with packaging/release/voxkit-pin (Linux release).
# Does not download model weights unless --download-models is passed.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-13.0}
pin=$(cat "$root/packaging/release/voxkit-pin")
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
git -C "$src" reset -q --hard
git -C "$src" checkout --detach "$pin"

(
    cd "$src/daemon"
    cargo build --release --locked
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
