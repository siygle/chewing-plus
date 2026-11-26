#!/bin/sh
# Build libvoxkit-mt.dylib, voxkit's optional translation plugin (llama.cpp
# with its own static ggml, CPU only), for this Mac.
# Needs macos/build-voxkit.sh to have fetched the voxkit source; the plugin
# source and the llama.cpp pin (tag + commit) come from that checkout, the
# same way packaging/release/build-arch.sh builds the Linux plugin.
# Does not download the translation model (voxkit does that on first use).
#
#   macos/build-voxkit-mt.sh
#
# Output: macos/build/voxkit-mt/libvoxkit-mt.dylib
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-13.0}
src=${VOXKIT_SRC:-"$root/macos/build/fcitx5-voxkit"}
out="$root/macos/build/voxkit-mt"
llama=${LLAMA_CPP_SRC:-"$root/macos/build/llama.cpp"}

for tool in cmake git; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool is required" >&2
        exit 1
    fi
done
if [ ! -f "$src/daemon/mt-plugin/LLAMA_PIN" ]; then
    echo "voxkit source not found at $src (run macos/build-voxkit.sh first)" >&2
    exit 1
fi
read -r tag commit < "$src/daemon/mt-plugin/LLAMA_PIN"

if [ ! -d "$llama/.git" ]; then
    git clone -q --depth 1 --branch "$tag" https://github.com/ggml-org/llama.cpp.git "$llama"
fi
if [ "$(git -C "$llama" rev-parse HEAD)" != "$commit" ]; then
    echo "llama.cpp at $llama is not $tag ($commit)" >&2
    exit 1
fi

# GGML_NATIVE=OFF: do not tune to the build machine (a CI M3/M4 would emit
# instructions an M1 lacks). Metal and BLAS stay off, as in the plugin's own
# CMake; Accelerate is a system framework.
cmake -S "$src/daemon/mt-plugin" -B "$out" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" \
    -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" \
    -DLLAMA_CPP_SOURCE_DIR="$llama" \
    -DGGML_NATIVE=OFF
cmake --build "$out" --config Release -j "$(sysctl -n hw.ncpu)"

lib="$out/libvoxkit-mt.dylib"
if [ ! -f "$lib" ]; then
    echo "build produced no $lib" >&2
    exit 1
fi
# Only the vk_mt_* C functions may be exported; ggml must stay private.
extra=$(nm -gU "$lib" | awk '{print $3}' | grep -v '^_vk_mt_' | grep . || true)
if [ -n "$extra" ]; then
    echo "libvoxkit-mt.dylib exports more than vk_mt_*:" >&2
    echo "$extra" >&2
    exit 1
fi
echo "libvoxkit-mt: $lib"
