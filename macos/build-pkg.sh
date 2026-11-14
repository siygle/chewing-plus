#!/bin/bash
# Build an unsigned installer. The .app inside is ad-hoc signed when
# codesign exists, so a local install can load it. productsign is not run.
#
#   macos/build-pkg.sh
#
# Output: macos/build/ChewingPlus.pkg
set -euo pipefail
if [[ "$(uname)" != Darwin ]]; then
    echo "macos/build-pkg.sh must run on macOS" >&2
    exit 1
fi
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
if [[ ! -d "$root/macos/build/ChewingPlus.app" ]]; then
    bash "$root/macos/build-app.sh"
fi
app="$root/macos/build/ChewingPlus.app"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/Library/Input Methods" "$root/macos/build"
cp -R "$app" "$stage/Library/Input Methods/ChewingPlus.app"
if command -v codesign >/dev/null 2>&1; then
    codesign --force --sign - "$stage/Library/Input Methods/ChewingPlus.app" || true
fi
pkgbuild --root "$stage" \
    --identifier im.chewing.plus.inputmethod \
    --version 5.1.13 \
    --scripts "$root/macos/pkg-scripts" \
    --install-location / \
    "$root/macos/build/ChewingPlus-component.pkg"
productbuild --package "$root/macos/build/ChewingPlus-component.pkg" \
    "$root/macos/build/ChewingPlus.pkg"
echo "pkg: $root/macos/build/ChewingPlus.pkg (unsigned)"
