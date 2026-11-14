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
exec "$bin" serve
