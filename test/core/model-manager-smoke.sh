#!/usr/bin/env bash
# Headless smoke test for chewing-plus-model-manager: the window opens under
# Xvfb with no voxkit daemon (offline `voxkit model list --json`), stays up,
# and GTK reports no criticals. Needs xvfb-run; ctest skips it otherwise.
#   model-manager-smoke.sh /path/to/chewing-plus-model-manager
set -u
manager=$1
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
cat >"$dir/voxkit" <<'FAKE'
#!/bin/sh
echo '{"v":1,"type":"model.list","req":0,"daemon":"offline","store":"/tmp/none","store_bytes":0,"free_bytes":1000000000,"model":"a","model_mt":"m","models":[{"id":"a","kind":"asr","purpose":"asr","title":"A","bytes":10,"installed":false,"active":true,"selectable":true,"restricted":false},{"id":"m","kind":"mt","purpose":"mt","title":"M","bytes":20,"installed":false,"active":true,"selectable":true,"restricted":true,"license_accepted":false,"restriction":"limited"}]}'
FAKE
chmod +x "$dir/voxkit"
env VOXKIT_BIN="$dir/voxkit" VOXKIT_SOCKET="$dir/none.sock" \
  XDG_DATA_HOME="$dir/data" HOME="$dir" GDK_BACKEND=x11 GSK_RENDERER=cairo \
  NO_AT_BRIDGE=1 G_DEBUG=fatal-criticals \
  xvfb-run -a timeout 4 "$manager" 2>"$dir/err"
status=$?
cat "$dir/err"
# 124: still running when timeout stopped it. Anything else is a crash.
if [ "$status" -ne 124 ]; then
  echo "FAIL: exit status $status (wanted 124: window stayed open)"
  exit 1
fi
echo "PASS: window stayed open for 4 s"
