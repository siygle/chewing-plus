#!/bin/sh
# Remove the input method from both per-user and system locations.
# Model weights under Application Support/models are left in place.
set -eu
home=${HOME:?}
uid=$(id -u)
launchctl bootout "gui/$uid/im.chewing.plus.voxkit" 2>/dev/null || true
rm -f "$home/Library/LaunchAgents/im.chewing.plus.voxkit.plist"
rm -rf "$home/Library/Input Methods/ChewingPlus.app"
rm -rf "/Library/Input Methods/ChewingPlus.app"
rm -f "$home/Library/Application Support/ChewingPlus/voxkit"
rm -f "$home/Library/Application Support/ChewingPlus/chewing-plus-models"
rm -f "$home/Library/Application Support/ChewingPlus/voxkit-launch.sh"
rm -rf "/Library/Application Support/ChewingPlus"
echo "removed ChewingPlus.app from ~/Library and /Library"
echo "model files were left in ~/Library/Application Support/ChewingPlus/models"
