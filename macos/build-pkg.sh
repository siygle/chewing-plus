#!/bin/bash
# Unsigned installer. The app is ad-hoc signed when codesign exists.
# The component is not relocatable: Installer must not move ChewingPlus.app
# onto some other bundle that happens to share the same id.
#
#   macos/build-pkg.sh
#
# Output: macos/build/ChewingPlus.pkg
# CHEWING_PLUS_VERSION sets the package and bundle version (default 6.1.0).
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
# Translation plugin (llama.cpp, CPU). Loaded by voxkit with dlopen only when
# someone translates; the HY-MT model is downloaded on first use, not bundled.
mt=${VOXKIT_MT_LIB:-"$root/macos/build/voxkit-mt/libvoxkit-mt.dylib"}
if [[ ! -f "$mt" ]]; then
    bash "$root/macos/build-voxkit-mt.sh"
    mt="$root/macos/build/voxkit-mt/libvoxkit-mt.dylib"
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
ditto --norsrc "$mt" "$support/libvoxkit-mt.dylib"
ditto --norsrc "$models" "$support/chewing-plus-models"
chmod 755 "$support/voxkit" "$support/chewing-plus-models"
chmod 644 "$support/libvoxkit-mt.dylib"
# 「新酷音 Plus 語音模型」 in /Applications: a script bundle that runs
# ChewingPlus --models (the model window on its own, with a Dock icon).
launcher="$stage/Applications/新酷音 Plus 語音模型.app"
mkdir -p "$launcher/Contents/MacOS" "$launcher/Contents/Resources"
ditto --norsrc "$root/macos/ModelsLauncher/Info.plist" "$launcher/Contents/Info.plist"
ditto --norsrc "$root/macos/ModelsLauncher/chewing-plus-models-window" \
    "$launcher/Contents/MacOS/chewing-plus-models-window"
ditto --norsrc "$root/macos/ChewingPlus/chewing-plus.icns" \
    "$launcher/Contents/Resources/chewing-plus.icns"
chmod 755 "$launcher/Contents/MacOS/chewing-plus-models-window"
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
ver=${CHEWING_PLUS_VERSION:-6.1.0}
if [[ -n "$ver" ]]; then
    # CFBundleShortVersionString allows at most three components.
    short=$(printf '%s' "$ver" | cut -d. -f1-3)
    /usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $short" \
        "$dest/Contents/Info.plist"
    /usr/libexec/PlistBuddy -c "Set :CFBundleVersion $ver" "$dest/Contents/Info.plist"
    /usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $short" \
        "$launcher/Contents/Info.plist"
    /usr/libexec/PlistBuddy -c "Set :CFBundleVersion $ver" "$launcher/Contents/Info.plist"
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
    --version "${CHEWING_PLUS_VERSION:-6.1.0}" \
    --scripts "$root/macos/pkg-scripts" \
    --install-location / \
    "$component"
# pkgbuild can still emit a <relocate> search (even an empty <relocate/>
# for a scripts-only package). Strip it and set the package attribute so
# Installer will not move the app.
strip_relocate() {
    local pkg=$1
    local expand_root expanded
    expand_root=$(mktemp -d "${TMPDIR:-/tmp}/chewing-plus-expand.XXXXXX")
    expanded="$expand_root/pkg"
    pkgutil --expand "$pkg" "$expanded"
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
    pkgutil --flatten "$expanded" "$pkg"
    rm -rf "$expand_root"
}
strip_relocate "$component"
# Optional "啟用語音" choice: an empty payload whose postinstall starts the
# voxkit agent. Unticked, voice stays off until enabled from the input menu.
voice_component="$root/macos/build/ChewingPlus-voice.pkg"
pkgbuild --nopayload \
    --identifier im.chewing.plus.voice \
    --version "${CHEWING_PLUS_VERSION:-6.1.0}" \
    --scripts "$root/macos/pkg-voice-scripts" \
    "$voice_component"
strip_relocate "$voice_component"
dist="$root/macos/build/distribution.xml"
cat > "$dist" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>新酷音 Plus</title>
    <options customize="always" require-scripts="false" hostArchitectures="arm64"/>
    <choices-outline>
        <line choice="main"/>
        <line choice="voice"/>
    </choices-outline>
    <choice id="main" title="新酷音 Plus 輸入法" enabled="false" selected="true"
            description="輸入法、詞庫與語音服務程式。">
        <pkg-ref id="im.chewing.plus.inputmethod"/>
    </choice>
    <choice id="voice" title="啟用語音" start_selected="false"
            description="安裝後立即啟動語音服務（按住右 Option 說話）。第一次使用要下載語音模型（約 850 MB），模型載入後約佔 260 MB 記憶體。之後也可以在輸入法選單開啟或關閉。">
        <pkg-ref id="im.chewing.plus.voice"/>
    </choice>
    <pkg-ref id="im.chewing.plus.inputmethod">ChewingPlus-component.pkg</pkg-ref>
    <pkg-ref id="im.chewing.plus.voice">ChewingPlus-voice.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution "$dist" --package-path "$root/macos/build" \
    "$root/macos/build/ChewingPlus.pkg"
echo "pkg: $root/macos/build/ChewingPlus.pkg (unsigned, not relocatable)"
