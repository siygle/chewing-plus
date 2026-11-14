#!/bin/sh
# Install ChewingPlus.app and the voxkit LaunchAgent.
# Substitutes __HOME__ in the plist. Does not download models unless
# --download-models is passed to build-voxkit first.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
app=${1:-"$root/macos/build/ChewingPlus.app"}
if [ ! -d "$app" ]; then
    echo "missing $app (run macos/build-app.sh on a Mac first)" >&2
    exit 1
fi
home=${HOME:?}
dest="$home/Library/Input Methods/ChewingPlus.app"
rm -rf "$dest"
mkdir -p "$home/Library/Input Methods" "$home/Library/LaunchAgents" \
    "$home/Library/Application Support/ChewingPlus/models"
cp -R "$app" "$dest"

# Helper binary: prefer a just-built voxkit, else leave the plist pointing
# at libexec and tell the user.
voxkit=${VOXKIT_BIN:-"$root/macos/build/fcitx5-voxkit/daemon/target/release/voxkit"}
helper_dest="$home/Library/Application Support/ChewingPlus/voxkit"
launch_dest="$home/Library/Application Support/ChewingPlus/voxkit-launch.sh"
cp "$root/macos/voxkit-launch.sh" "$launch_dest"
chmod 755 "$launch_dest"
if [ -x "$voxkit" ]; then
    cp "$voxkit" "$helper_dest"
    chmod 755 "$helper_dest"
else
    echo "voxkit binary not found at $voxkit"
    echo "run: macos/build-voxkit.sh"
    echo "the LaunchAgent exits quietly until that binary exists"
fi

agent="$home/Library/LaunchAgents/im.chewing.plus.voxkit.plist"
sed -e "s|__HOME__|$home|g" \
    "$root/macos/launchd/im.chewing.plus.voxkit.plist" > "$agent"
uid=$(id -u)
domain="gui/$uid"
launchctl bootout "$domain/im.chewing.plus.voxkit" 2>/dev/null || true
launchctl bootstrap "$domain" "$agent" || launchctl load -w "$agent"
echo "installed $dest"
echo "enable 新酷音 Plus in System Settings → Keyboard → Input Sources"
echo "models: $home/Library/Application Support/ChewingPlus/models"
echo "  \"$helper_dest\" model install qwen3-asr-0.6b"
