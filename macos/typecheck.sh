#!/bin/sh
# Headless checks for the macOS runner. No signing, no model, no libchewing.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"

clang -std=c99 -Wall -Wextra -Werror -I include \
    -o /tmp/voxkit-protocol-test \
    test/core/test_voxkit_protocol.c src/voxkit/protocol.c
/tmp/voxkit-protocol-test

clang -std=c99 -Wall -Wextra -Werror -D_DARWIN_C_SOURCE=1 -D_POSIX_C_SOURCE=200809L -I include \
    -o /tmp/voxkit-client-test \
    test/core/test_voxkit_client.c src/voxkit/protocol.c src/voxkit/client.c
/tmp/voxkit-client-test

clang -std=c99 -Wall -Wextra -Werror -D_DARWIN_C_SOURCE=1 -D_POSIX_C_SOURCE=200809L \
    -I src/ibus \
    -o /tmp/ibus-prefs-test \
    test/core/test_ibus_prefs.c src/ibus/prefs.c
/tmp/ibus-prefs-test

sdk=$(xcrun --sdk macosx --show-sdk-path)
xcrun swiftc -typecheck \
    -sdk "$sdk" \
    -import-objc-header macos/ChewingPlus/ChewingPlus-Bridging-Header.h \
    -Xcc -I -Xcc include \
    macos/ChewingPlus/main.swift \
    macos/ChewingPlus/ChewingPlusInputController.swift \
    macos/ChewingPlus/ModelPreferences.swift
echo "macOS typecheck OK"
