#!/usr/bin/env bash
# Chewing + fcitx5-voxkit end-to-end test with a fake daemon, fully isolated.
#   test/run-voxkit-test.sh BUILD_DIR CHEWING_DATA_DIR LIBCHEWING_LIB_DIR VOXKIT_BUILD_DIR VOXKIT_SRC_DIR
set -euo pipefail
build=$1 data=$2 lib=$3 vkbuild=$4 vksrc=$5
root=$(mktemp -d)
trap 'kill $fake 2>/dev/null || true; rm -rf "$root"' EXIT
mkdir -p "$root/home" "$root/share/libchewing" "$root/vkdata/addon" "$root/config"
cp "$data"/* "$root/share/libchewing/"
cp "$vkbuild/module/voxkit.conf" "$root/vkdata/addon/voxkit.conf"
text="今天天氣很好"
python3 -I "$vksrc/test/fake_voxkit.py" "$root/voxkit.sock" "$text" "$root/fake.log" &
fake=$!
for _ in $(seq 50); do [[ -e $root/voxkit.sock.ready ]] && break; sleep 0.05; done
env -i PATH="$PATH" HOME="$root/home" \
  XDG_DATA_HOME="$root/home/.local/share" XDG_CONFIG_HOME="$root/config" \
  XDG_RUNTIME_DIR="$root" XDG_DATA_DIRS="$root/share" \
  CHEWING_PATH="$root/share/libchewing" CHEWING_USER_PATH="$root/home/chewing" \
  VOXKIT_SOCKET="$root/voxkit.sock" VOXKIT_ADDON_DIR="$vkbuild/module" \
  VOXKIT_DATA_DIR="$root/vkdata" FAKE_TEXT="$text" FAKE_LOG="$root/fake.log" \
  LD_LIBRARY_PATH="$lib" \
  "$build/test/testvoxkit"
