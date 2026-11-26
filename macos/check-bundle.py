#!/usr/bin/env python3
"""Validate the IMK plist and, when given, the built app and pkg.

Source mode checks the files in the repo. App and pkg mode check the
artifacts macos/build-pkg.sh produces. The pkg must install
ChewingPlus.app at /Library/Input Methods and must not be relocatable.
"""
import argparse
import plistlib
import subprocess
import sys
import tempfile
from pathlib import Path

BUNDLE_ID = "im.chewing.inputmethod.ChewingPlus"
MODE = BUNDLE_ID + ".Bopomofo"
MODE_KEYS = (
    "TISInputSourceID",
    "TISIntendedLanguage",
    "tsInputModeScriptKey",
    "tsInputModePrimaryInScriptKey",
    "tsInputModeMenuIconFileKey",
    "tsInputModePaletteIconFileKey",
    "tsInputModeDefaultStateKey",
)


def fail(message):
    print("check-bundle: " + message, file=sys.stderr)
    return 1


def load_plist(path):
    with open(path, "rb") as fh:
        return plistlib.load(fh)


def check_dict(info, resource_root, label):
    errors = []
    if info.get("CFBundlePackageType") != "APPL":
        errors.append(label + " CFBundlePackageType is not APPL")
    if info.get("LSUIElement") is not True:
        errors.append(label + " LSUIElement is not true")
    bundle_id = info.get("CFBundleIdentifier", "")
    if bundle_id != BUNDLE_ID:
        errors.append(label + " CFBundleIdentifier is not " + BUNDLE_ID)
    # Text Input Sources only indexes a bundle whose id has ".inputmethod."
    # in the middle. Without it TISRegisterInputSource returns noErr and the
    # input method never shows up (issue #7).
    if ".inputmethod." not in bundle_id:
        errors.append(label + " CFBundleIdentifier has no '.inputmethod.' component")
    if info.get("TISInputSourceID") != BUNDLE_ID:
        errors.append(label + " TISInputSourceID")
    if info.get("TISIntendedLanguage") != "zh-Hant":
        errors.append(label + " TISIntendedLanguage")
    if info.get("InputMethodServerControllerClass") != "ChewingPlusInputController":
        errors.append(label + " controller class")
    if info.get("InputMethodConnectionName") != "im.chewing.inputmethod.ChewingPlus.Connection":
        errors.append(label + " connection name")
    icon = info.get("tsInputMethodIconFileKey")
    if not icon:
        errors.append(label + " missing tsInputMethodIconFileKey")
    elif not (resource_root / icon).is_file():
        errors.append(label + " icon missing: " + icon)
    modes = info.get("ComponentInputModeDict") or {}
    listing = (modes.get("tsInputModeListKey") or {}).get(MODE)
    visible = modes.get("tsVisibleInputModeOrderedArrayKey") or []
    if MODE not in visible:
        errors.append(label + " mode is not in the visible list")
    if not isinstance(listing, dict):
        errors.append(label + " missing mode " + MODE)
    else:
        for key in MODE_KEYS:
            if key not in listing:
                errors.append(label + " mode missing " + key)
        if listing.get("TISInputSourceID") != MODE:
            errors.append(label + " mode TISInputSourceID")
        if listing.get("TISIntendedLanguage") != "zh-Hant":
            errors.append(label + " mode language")
        if listing.get("tsInputModeScriptKey") != "smTradChinese":
            errors.append(label + " mode script")
        if listing.get("tsInputModePrimaryInScriptKey") is not True:
            errors.append(label + " mode is not primary")
        for key in ("tsInputModeMenuIconFileKey", "tsInputModePaletteIconFileKey"):
            name = listing.get(key)
            if name and not (resource_root / name).is_file():
                errors.append(label + " mode icon missing: " + str(name))
    strings = resource_root / "zh-Hant.lproj" / "InfoPlist.strings"
    if not strings.is_file():
        errors.append(label + " missing zh-Hant InfoPlist.strings")
    else:
        text = strings.read_text(encoding="utf-8")
        if "新酷音 Plus" not in text or MODE not in text:
            errors.append(label + " strings do not name 新酷音 Plus")
    return errors


def payload_listing(expanded, pkg):
    chunks = []
    for bom in expanded.rglob("Bom"):
        chunks.append(
            subprocess.check_output(
                ["lsbom", str(bom)], text=True, errors="replace"
            )
        )
    if chunks:
        return "\n".join(chunks)
    return subprocess.check_output(
        ["pkgutil", "--payload-files", str(pkg)], text=True, errors="replace"
    )


def check_pkg(pkg):
    errors = []
    with tempfile.TemporaryDirectory() as tmp:
        expanded = Path(tmp) / "expanded"
        subprocess.check_call(["pkgutil", "--expand", str(pkg), str(expanded)])
        infos = list(expanded.rglob("PackageInfo"))
        if not infos:
            return ["pkg has no PackageInfo"]
        text = "\n".join(
            path.read_text(encoding="utf-8", errors="replace") for path in infos
        )
        needle = "Library/Input Methods/ChewingPlus.app"
        if needle not in text:
            errors.append("pkg PackageInfo does not install " + needle)
        if "<relocate" in text or 'relocatable="true"' in text:
            errors.append("pkg is relocatable")
        if 'relocatable="false"' not in text:
            errors.append("pkg PackageInfo is missing relocatable=false")
        listed = payload_listing(expanded, pkg)
        paths = []
        for line in listed.splitlines():
            path = line.split("\t", 1)[0].strip()
            if path:
                paths.append(path)
        if not any(needle in path for path in paths):
            errors.append("pkg payload is missing /" + needle)
        apps = [path for path in paths if path.rstrip("/").endswith("ChewingPlus.app")]
        if len(apps) != 1:
            errors.append("pkg payload has %d ChewingPlus.app entries" % len(apps))
        plugin = "Library/Application Support/ChewingPlus/libvoxkit-mt.dylib"
        if not any(plugin in path for path in paths):
            errors.append("pkg payload is missing /" + plugin)
        launcher = "Applications/新酷音 Plus 語音模型.app/Contents/MacOS/chewing-plus-models-window"
        if not any(launcher in path for path in paths):
            errors.append("pkg payload is missing the model window launcher /" + launcher)
        if any("/._" in path or path.startswith("._") for path in paths):
            errors.append("pkg payload contains AppleDouble files")
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=None)
    parser.add_argument("--app", type=Path, default=None)
    parser.add_argument("--pkg", type=Path, default=None)
    parser.add_argument("--component-plist", type=Path, default=None)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    errors = []
    if args.source:
        source = args.source
        errors.extend(check_dict(load_plist(source / "macos/ChewingPlus/Info.plist"),
                                 source / "macos/ChewingPlus", "source"))
    if args.app:
        errors.extend(check_dict(load_plist(args.app / "Contents/Info.plist"),
                                 args.app / "Contents/Resources", "app"))
    if args.component_plist:
        rows = load_plist(args.component_plist)
        if any(row.get("BundleIsRelocatable") for row in rows):
            errors.append("component plist still relocatable")
        paths = [row.get("RootRelativeBundlePath", "") for row in rows]
        if "Library/Input Methods/ChewingPlus.app" not in paths:
            errors.append("component plist path is not /Library/Input Methods/ChewingPlus.app")
    if args.pkg:
        errors.extend(check_pkg(args.pkg))
    if not any((args.source, args.app, args.pkg, args.component_plist)):
        errors.extend(check_dict(load_plist(root / "macos/ChewingPlus/Info.plist"),
                                 root / "macos/ChewingPlus", "source"))
    if errors:
        for item in errors:
            fail(item)
        return 1
    print("check-bundle: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
