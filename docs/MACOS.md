# macOS InputMethodKit app

This is the native macOS frontend. It is AppKit and InputMethodKit only.
There is no Qt and no shared UI toolkit. The composition engine is
`libchewing-plus-core`. Voice is option C from
[CROSS_PLATFORM.md](CROSS_PLATFORM.md): a launchd helper that is the voxkit
daemon (Qwen3-ASR, Silero VAD, OpenCC `s2twp`, all local). The input method
only speaks [VOXKIT_PROTOCOL.md](VOXKIT_PROTOCOL.md) version 1.

## What is in this tree

| Path | State |
|---|---|
| `macos/ChewingPlus/*.swift` | IMK controller. Right Option is push-to-talk. Marked text comes from `chewing_plus_preedit`. A `final` event calls `chewing_plus_insert_text`. |
| `macos/ChewingPlus/Info.plist` | Bundle id `im.chewing.plus.inputmethod`. |
| `macos/launchd/im.chewing.plus.voxkit.plist` | User agent. Socket `/tmp/chewing-plus-voxkit.sock`. Model `qwen3-asr-0.6b`. |
| `macos/typecheck.sh` | What CI runs on the macOS runner. |

Plus features stay off unless `CHEWING_PLUS_FEATURES` lists the same tokens
as the IBus engine. The fcitx5 addon is not involved.

## What is not done

The voxkit **daemon binary is not built or vendored here**. It lives in
[fcitx5-voxkit](https://github.com/siygle/fcitx5-voxkit) `daemon/` (Rust,
transcribe-cpp, cpal, OpenCC via `dlopen` of `s2twp.json`). That stack is
the one the owner chose; copying it in would not add a new kind of
dependency, but the Linux `dlopen` paths and the ALSA notes do not apply on
macOS. Remaining work, in order:

1. Build `voxkit` on macOS. cpal already has a CoreAudio backend. Point
   OpenCC at a Homebrew or bundled `s2twp.json` plus `libopencc`, or link
   OpenCC statically. Do not download model weights inside the input method.
2. Replace `__HOME__` in the plist at install time. `launchctl bootstrap
   gui/$UID ~/Library/LaunchAgents/im.chewing.plus.voxkit.plist`.
3. Link `ChewingPlus.app` against `libchewing-plus-core.a` and a bundled
   libchewing 0.14 (`scripts/bundle-libchewing.sh --shared`, Rust 1.85+,
   git-lfs). Ship `static_dict.bin` inside the app and pass its directory
   to `chewing_plus_new`.
4. Draw candidates with `IMKCandidates` (still AppKit). The controller
   currently reads the first candidate and does not show the panel yet.
5. Code signing and notarization. See below. Not done in CI.

`macos/typecheck.sh` typechecks the Swift against the C headers and runs
the protocol and fake-daemon tests. A `macos-14` job was requested on this
PR. GitHub cancelled it before assigning a runner (the job has no steps and
an empty runner name), so the workflow does not depend on that job. Run the
script on a Mac when a runner is available. It does not produce a signed
`.app` and it does not load Qwen.

## Apple Developer setup

Notarization needs an Apple Developer Program membership. CI does not have
those secrets.

1. Identifiers: `im.chewing.plus.inputmethod` for the app,
   `im.chewing.plus.voxkit` for the helper.
2. The helper needs the microphone. Entitlement
   `com.apple.security.device.audio-input` if it is sandboxed. Unsandboxed,
   the first `cpal` open triggers the system prompt. The prompt is attributed
   to the helper, not the input method, because the helper opens the device.
3. Install the app at `~/Library/Input Methods/ChewingPlus.app`, then log out
   or `killall ChewingPlus` and enable it in System Settings → Keyboard →
   Input Sources. Input methods are not notarized App Store apps; Developer
   ID Application plus notarization is what Gatekeeper accepts for a
   download.
4. Sign:

   ```bash
   codesign --force --options runtime --timestamp \
     --sign "Developer ID Application: <Team>" \
     ChewingPlus.app
   codesign --force --options runtime --timestamp \
     --sign "Developer ID Application: <Team>" \
     voxkit
   ```

5. Notarize the zip or dmg:

   ```bash
   xcrun notarytool submit ChewingPlus.zip --keychain-profile AC_NOTARY --wait
   xcrun stapler staple ChewingPlus.app
   ```

   `AC_NOTARY` is an app-specific password or an App Store Connect API key
   stored with `notarytool store-credentials`. Do not put that secret in
   this repo.

6. Hardened runtime plus the microphone entitlement means the helper's
   entitlements file must list `com.apple.security.device.audio-input` and,
   if the dictionary is outside the bundle, a read exception for
   `~/Library/Application Support/ChewingPlus`.
