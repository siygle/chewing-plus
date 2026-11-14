#!/bin/sh
set -eu
home=${HOME:?}
uid=$(id -u)
launchctl bootout "gui/$uid/im.chewing.plus.voxkit" 2>/dev/null || true
rm -f "$home/Library/LaunchAgents/im.chewing.plus.voxkit.plist"
rm -rf "$home/Library/Input Methods/ChewingPlus.app"
rm -f "$home/Library/Application Support/ChewingPlus/voxkit"
echo "removed the input method and the LaunchAgent"
echo "model files were left in ~/Library/Application Support/ChewingPlus/models"
