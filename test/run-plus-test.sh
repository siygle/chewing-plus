#!/usr/bin/env bash
# Run testplus with an isolated HOME/XDG dirs and only the given libchewing
# data dir, so the real ~/.local/share/chewing is never read or migrated.
#   test/run-plus-test.sh BUILD_DIR CHEWING_DATA_DIR [LIBCHEWING_LIB_DIR]
set -euo pipefail
build=$1
data=$2
lib=${3:-}
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
mkdir -p "$root/home" "$root/share/libchewing" "$root/share/fcitx5/data" "$root/config"
cp "$data"/* "$root/share/libchewing/"
src=$(cd "$(dirname "$0")/.." && pwd)
cp "$src/data/plus-emoji-shortcodes.txt" "$root/share/fcitx5/data/chewing-plus-emoji-shortcodes.txt"
cp "$src/data/plus-english.txt" "$root/share/fcitx5/data/chewing-plus-english.txt"
cp "$src/data/plus-phrases.txt" "$root/share/fcitx5/data/chewing-plus-phrases.txt"
env -i PATH="$PATH" HOME="$root/home" \
  XDG_DATA_HOME="$root/home/.local/share" XDG_CONFIG_HOME="$root/config" \
  XDG_RUNTIME_DIR="$root" XDG_DATA_DIRS="$root/share" \
  CHEWING_PATH="$root/share/libchewing" CHEWING_USER_PATH="$root/home/chewing" \
  CHEWING_TEST_DATA="$data" \
  PLUS_TEST_FCITX_DATA="$root/share/fcitx5" \
  ${lib:+LD_LIBRARY_PATH="$lib"} \
  "$build/test/testplus"
# Upstream test, same isolation. setupTestingEnvironment hides system data
# dirs, so the plugin falls back to chewing_new(); CHEWING_PATH points it at
# the 0.14 data instead of /usr/share/libchewing.
env -i PATH="$PATH" HOME="$root/home" \
  XDG_DATA_HOME="$root/home/.local/share" XDG_CONFIG_HOME="$root/config" \
  XDG_RUNTIME_DIR="$root" XDG_DATA_DIRS="$root/share" \
  CHEWING_PATH="$root/share/libchewing" CHEWING_USER_PATH="$root/home/chewing2" \
  CHEWING_TEST_DATA="$data" \
  ${lib:+LD_LIBRARY_PATH="$lib"} \
  "$build/test/testchewing" >/dev/null 2>&1 && echo "UPSTREAM TEST OK" || { echo "UPSTREAM TEST FAILED"; exit 1; }
