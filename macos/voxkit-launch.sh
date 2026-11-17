#!/bin/sh
# launchd entry. A missing binary exits 0 so KeepAlive does not restart
# in a loop. A present binary stays up even when no model is installed;
# prewarm logs and the socket still answers model.* commands.
set -eu
home=${HOME:?}
bin="$home/Library/Application Support/ChewingPlus/voxkit"
if [ ! -x "$bin" ]; then
    echo "voxkit: binary missing at $bin; not restarting" >&2
    exit 0
fi
# launchd gives a bare PATH (/usr/bin:/bin:...). voxkit finds OpenCC for the
# Simplified -> Traditional (s2twp) step through PATH and OPENCC_DATA_DIR; without
# Homebrew here every transcript stays Simplified (Qwen3-ASR emits Simplified).
for prefix in /opt/homebrew /usr/local; do
    if [ -x "$prefix/bin/opencc" ]; then
        PATH="$prefix/bin:$PATH"
        if [ -z "${OPENCC_DATA_DIR:-}" ] && [ -f "$prefix/share/opencc/s2twp.json" ]; then
            OPENCC_DATA_DIR="$prefix/share/opencc"
        fi
    fi
done
export PATH
if [ -n "${OPENCC_DATA_DIR:-}" ]; then
    export OPENCC_DATA_DIR
fi
exec "$bin" serve
