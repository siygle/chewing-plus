#!/bin/sh
# Assemble ChewingPlus.app without an Xcode project.
# Runs only on macOS. Links the C ABI and a bundled libchewing 0.14 dylib.
set -eu
if [ "$(uname)" != Darwin ]; then
    echo "macos/build-app.sh produces a .app and must run on macOS" >&2
    exit 1
fi
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
# Oldest macOS the app runs on. CMake, cargo and swiftc all read this, so the
# runner's own macOS version does not leak into the bundle.
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-13.0}
prefix=${CHEWING_PREFIX:-"$root/macos/build/libchewing"}
if [ ! -f "$prefix/lib/libchewing.dylib" ] && [ ! -f "$prefix/lib/libchewing.3.dylib" ]; then
    bash "$root/scripts/bundle-libchewing.sh" --shared "$prefix"
fi
# libchewing's shared library name.
libchewing=$(ls "$prefix"/lib/libchewing.dylib "$prefix"/lib/libchewing.*.dylib 2>/dev/null | head -1)
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake -S "$root" -B "$root/macos/build/core" -G Ninja \
    -DCHEWING_PLUS_CORE_ONLY=ON -DENABLE_TEST=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCHEWING_PLUS_BUNDLE_LIBCHEWING=ON \
    -DCHEWING_PLUS_LIBCHEWING_PREFIX="$prefix" \
    || cmake -S "$root" -B "$root/macos/build/core" \
        -DCHEWING_PLUS_CORE_ONLY=ON -DENABLE_TEST=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DCHEWING_PLUS_BUNDLE_LIBCHEWING=ON \
        -DCHEWING_PLUS_LIBCHEWING_PREFIX="$prefix"
cmake --build "$root/macos/build/core"

app="$root/macos/build/ChewingPlus.app"
rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Frameworks" "$app/Contents/Resources/libchewing"
cp "$root/macos/ChewingPlus/Info.plist" "$app/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :LSMinimumSystemVersion $MACOSX_DEPLOYMENT_TARGET" \
    "$app/Contents/Info.plist"
cp "$root/macos/ChewingPlus/chewing-plus.pdf" "$root/macos/ChewingPlus/chewing-plus.tiff" \
    "$root/macos/ChewingPlus/chewing-plus.icns" "$app/Contents/Resources/"
mkdir -p "$app/Contents/Resources/zh-Hant.lproj" "$app/Contents/Resources/Base.lproj"
cp "$root/macos/ChewingPlus/zh-Hant.lproj/InfoPlist.strings" \
    "$app/Contents/Resources/zh-Hant.lproj/"
cp "$root/macos/ChewingPlus/Base.lproj/InfoPlist.strings" \
    "$app/Contents/Resources/Base.lproj/"
cp "$prefix/share/libchewing/"*.bin "$prefix/share/libchewing/"*.dat \
    "$app/Contents/Resources/libchewing/" 2>/dev/null || true
cp "$root/data/plus-emoji-shortcodes.txt" "$root/data/plus-english.txt" \
    "$root/macos/build/core/plus-phrases.bin" \
    "$app/Contents/Resources/"
cp "$libchewing" "$app/Contents/Frameworks/libchewing.dylib"
# Also copy the versioned name the linker recorded, if it is a separate file.
find "$prefix/lib" -name 'libchewing*.dylib' -exec cp -a {} "$app/Contents/Frameworks/" \;

sdk=$(xcrun --sdk macosx --show-sdk-path)
xcrun swiftc -O -emit-executable \
    -sdk "$sdk" \
    -target "$(uname -m)-apple-macos$MACOSX_DEPLOYMENT_TARGET" \
    -import-objc-header "$root/macos/ChewingPlus/ChewingPlus-Bridging-Header.h" \
    -Xcc -I -Xcc "$root/include" \
    -Xlinker -L"$root/macos/build/core/src" \
    -Xlinker -L"$app/Contents/Frameworks" \
    -Xlinker -lchewing-plus-core \
    -Xlinker -lvoxkit-protocol \
    -Xlinker -lchewing \
    -Xlinker -lc++ \
    -Xlinker -rpath -Xlinker @executable_path/../Frameworks \
    -framework Cocoa -framework InputMethodKit -framework Carbon \
    -o "$app/Contents/MacOS/ChewingPlus" \
    "$root/macos/ChewingPlus/main.swift" \
    "$root/macos/ChewingPlus/ChewingPlusInputController.swift" \
    "$root/macos/ChewingPlus/ModelPreferences.swift" \
    "$root/macos/ChewingPlus/Shortcuts.swift"

install_name_tool -add_rpath @executable_path/../Frameworks \
    "$app/Contents/MacOS/ChewingPlus" || true
echo "app: $app"
echo "next: macos/sign.sh \"$app\" && macos/install.sh"
