#!/bin/bash
# Unsigned installer. The app is ad-hoc signed when codesign exists.
# The component is not relocatable: Installer must not move ChewingPlus.app
# onto some other bundle that happens to share the same id.
#
#   macos/build-pkg.sh
#
# Output: macos/build/ChewingPlus.pkg
set -euo pipefail
export COPYFILE_DISABLE=1
if [[ "$(uname)" != Darwin ]]; then
    echo "macos/build-pkg.sh must run on macOS" >&2
    exit 1
fi
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
if [[ ! -d "$root/macos/build/ChewingPlus.app" ]]; then
    bash "$root/macos/build-app.sh"
fi
app="$root/macos/build/ChewingPlus.app"
models="$root/macos/build/core/src/chewing-plus-models"
if [[ ! -x "$models" ]]; then
    cmake --build "$root/macos/build/core" --target chewing-plus-models
fi
voxkit=${VOXKIT_BIN:-"$root/macos/build/fcitx5-voxkit/daemon/target/release/voxkit"}
if [[ ! -x "$voxkit" ]]; then
    bash "$root/macos/build-voxkit.sh"
    voxkit="$root/macos/build/fcitx5-voxkit/daemon/target/release/voxkit"
fi

# Staging lives outside macos/build so the tree that produced the app is
# not also inside the payload. Only one ChewingPlus.app is copied, at the
# install path.
stage=$(mktemp -d "${TMPDIR:-/tmp}/chewing-plus-pkg.XXXXXX")
trap 'rm -rf "$stage"' EXIT
dest="$stage/Library/Input Methods/ChewingPlus.app"
support="$stage/Library/Application Support/ChewingPlus"
mkdir -p "$dest" "$support" "$root/macos/build"
ditto --norsrc "$app" "$dest"
ditto --norsrc "$voxkit" "$support/voxkit"
ditto --norsrc "$models" "$support/chewing-plus-models"
chmod 755 "$support/voxkit" "$support/chewing-plus-models"
find "$stage" -name '._*' -delete
find "$stage" -name '.DS_Store' -delete
if command -v dot_clean >/dev/null 2>&1; then
    dot_clean -m "$stage" >/dev/null
fi
# The source app stays in macos/build and is not part of this root.
apps=$(find "$stage" -name 'ChewingPlus.app' -type d | wc -l | tr -d ' ')
if [[ "$apps" != 1 ]]; then
    echo "payload has $apps ChewingPlus.app bundles; expected 1" >&2
    exit 1
fi
if command -v codesign >/dev/null 2>&1; then
    codesign --force --sign - "$dest" || true
fi

plist="$root/macos/build/component.plist"
pkgbuild --analyze --root "$stage" "$plist"
python3 - "$plist" <<'PY'
import plistlib
import sys
path = sys.argv[1]
with open(path, "rb") as fh:
    rows = plistlib.load(fh)
if not rows:
    raise SystemExit("pkgbuild --analyze produced an empty component list")
for row in rows:
    row["BundleIsRelocatable"] = False
with open(path, "wb") as fh:
    plistlib.dump(rows, fh)
PY

component="$root/macos/build/ChewingPlus-component.pkg"
pkgbuild --root "$stage" \
    --component-plist "$plist" \
    --identifier im.chewing.plus.inputmethod \
    --version 5.1.13 \
    --scripts "$root/macos/pkg-scripts" \
    --install-location / \
    "$component"
# pkgbuild can still emit a <relocate> search. Strip it and set the
# package attribute so Installer will not move the app.
expanded=$(mktemp -d "${TMPDIR:-/tmp}/chewing-plus-expand.XXXXXX")
pkgutil --expand "$component" "$expanded"
python3 - "$expanded/PackageInfo" <<'PY'
import re
import sys
from pathlib import Path
path = Path(sys.argv[1])
text = path.read_text()
text = re.sub(r"<relocate\b[^>]*/>", "", text)
text = re.sub(r"<relocate\b[^>]*>.*?</relocate>", "", text, flags=re.S)
text = text.replace('relocatable="true"', 'relocatable="false"')
if "relocatable=" not in text:
    text = text.replace("<pkg-info ", '<pkg-info relocatable="false" ', 1)
path.write_text(text)
PY
pkgutil --flatten "$expanded" "$component"
rm -rf "$expanded"
productbuild --package "$component" \
    "$root/macos/build/ChewingPlus.pkg"
echo "pkg: $root/macos/build/ChewingPlus.pkg (unsigned, not relocatable)"
