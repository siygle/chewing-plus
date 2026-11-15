#!/bin/sh
# Sign ChewingPlus.app and, if present, the voxkit helper.
# APPLE_SIGN_ID="Developer ID Application: Name (TEAMID)" uses that identity
# and, when NOTARY_KEYCHAIN_PROFILE is set, notarizes. Otherwise ad-hoc (-).
set -eu
app=${1:-}
if [ -z "$app" ] || [ ! -d "$app" ]; then
    echo "usage: macos/sign.sh ChewingPlus.app [voxkit-binary-or-app]" >&2
    exit 1
fi
helper=${2:-}
id=${APPLE_SIGN_ID:-}
if [ -n "$id" ]; then
    sign() { codesign --force --options runtime --timestamp --sign "$id" "$@"; }
else
    echo "APPLE_SIGN_ID is unset; ad-hoc signature. Gatekeeper will refuse a downloaded copy."
    sign() { codesign --force --sign - "$@"; }
fi
for lib in "$app/Contents/Frameworks"/*.dylib; do
    if [ -f "$lib" ]; then
        sign "$lib"
    fi
done
sign "$app"
if [ -n "$helper" ] && [ -e "$helper" ]; then
    sign "$helper"
fi
if [ -n "$id" ] && [ -n "${NOTARY_KEYCHAIN_PROFILE:-}" ]; then
    zip=$(mktemp -d)/ChewingPlus.zip
    ditto -c -k --keepParent "$app" "$zip"
    xcrun notarytool submit "$zip" --keychain-profile "$NOTARY_KEYCHAIN_PROFILE" --wait
    xcrun stapler staple "$app"
    echo "notarized $app"
else
    echo "signed $app (notarization skipped)"
fi
