# macOS InputMethodKit app

Native macOS frontend. AppKit and InputMethodKit only: no Qt and no shared
UI toolkit. Composition is `libchewing-plus-core`. Voice is the voxkit
daemon (Qwen3-ASR, Silero VAD, OpenCC `s2twp`, all local) started by
launchd. The input method speaks [VOXKIT_PROTOCOL.md](VOXKIT_PROTOCOL.md)
version 1 and does not load a model.

The fcitx5 addon is not part of this build.

## Layout

| Path | Role |
|---|---|
| `macos/ChewingPlus/ChewingPlusInputController.swift` | IMK controller: marked text, `IMKCandidates`, digit selection, English toggle via the core, secure input, Right Option push-to-talk |
| `macos/ChewingPlus/Info.plist` | Bundle id `im.chewing.plus.inputmethod`, IMK keys, microphone usage string |
| `macos/ChewingPlus/main.swift` | `IMKServer` named `ChewingPlus_Connection` |
| `macos/Voxkit/Info.plist` | Usage string for a helper bundle, if you wrap the daemon |
| `macos/launchd/im.chewing.plus.voxkit.plist` | User agent. `__HOME__` is replaced by `install.sh` |
| `macos/patches/voxkit-macos-opencc.patch` | Teaches fcitx5-voxkit `9acbb719` to find Homebrew OpenCC and `libopencc*.dylib` |
| `macos/build-voxkit.sh` | Clone that commit, apply the patch, `cargo build --release` |
| `macos/ChewingPlus/ModelPreferences.swift` | AppKit window: list, download, cancel, delete, select, disk use, daemon status |
| `macos/build-app.sh` | On macOS: bundle libchewing 0.14, build the core, emit `ChewingPlus.app` |
| `macos/build-pkg.sh` | Unsigned `.pkg` (ad-hoc signature on the `.app` when `codesign` exists) |
| `macos/sign.sh` | Developer ID if `APPLE_SIGN_ID` is set, otherwise ad-hoc `codesign -` |
| `macos/install.sh` / `macos/uninstall.sh` | `~/Library/Input Methods` and LaunchAgents |
| `macos/typecheck.sh` | Protocol tests plus `swiftc -typecheck` (no `.app`) |

Plus features stay off unless `CHEWING_PLUS_FEATURES` (or
`CHEWING_PLUS_IBUS_FEATURES`) contains the same tokens as the IBus engine:
`emoji`, `calculator`, `space`, `shortcode`, `english`.

## Build on a Mac

Needs Xcode command-line tools (`xcrun`, `swiftc`, `clang`), Rust 1.85+,
CMake, Ninja, git-lfs. OpenCC is the same library the daemon already uses;
Homebrew's `opencc` formula supplies `libopencc` and `s2twp.json`. That is
not a new kind of dependency. Do not add another converter.

```bash
# 1. Daemon (existing fcitx5-voxkit tree, patched). Weights are optional.
macos/build-voxkit.sh
# or: macos/build-voxkit.sh --download-models
#     (~850 MB, Qwen3-ASR-0.6B Q8, stays under Application Support)

# 2. Input method. Bundles libchewing 0.14 and static_dict.bin.
macos/build-app.sh

# 3. Sign. Ad-hoc when APPLE_SIGN_ID is unset.
macos/sign.sh macos/build/ChewingPlus.app \
    macos/build/fcitx5-voxkit/daemon/target/release/voxkit

# 4. Install (rewrites __HOME__, bootstraps launchd).
macos/install.sh

# 5. Optional unsigned package. Ad-hoc signs the .app, not the pkg.
bash macos/build-pkg.sh
```

The input-source menu item 语音模型… opens the AppKit catalog. It calls
the same C client as `chewing-plus-models`. The LaunchAgent runs
`voxkit-launch.sh`. If the daemon binary is missing the wrapper exits 0,
and `KeepAlive` only restarts a non-zero exit, with a 30 second throttle,
so an empty model store does not respawn the job in a loop. The daemon
stays up without weights and answers `model.*`.

`macos/build-pkg.sh` installs the app at `/Library/Input Methods/ChewingPlus.app` and marks that bundle non-relocatable, so Installer does not move it onto another copy with the same id. The package also carries `voxkit` and `chewing-plus-models`; the postinstall copies them into the console user's `~/Library/Application Support/ChewingPlus`. `macos/check-bundle.py` checks the IMK keys and that path.

`build-app.sh` refuses to run on Linux. `libchewing` 0.14.0-rc.1's CMake
cannot emit a static archive (the `chewing_capi` export error), so the app
bundles the shared `libchewing.dylib` plus `static_dict.bin` and the Plus
word lists under `Contents/Resources`.

The patch was applied to commit `9acbb719` and `cargo check` of that daemon
succeeded on Linux (the macOS `cfg` branch is not compiled there). A Mac
build of the daemon still has to be run on a Mac because cpal links
CoreAudio.

## Install layout

- `~/Library/Input Methods/ChewingPlus.app`
- `~/Library/LaunchAgents/im.chewing.plus.voxkit.plist`
- `~/Library/Application Support/ChewingPlus/voxkit` (the daemon binary)
- `~/Library/Application Support/ChewingPlus/models` (weights; not deleted by uninstall)

Socket after install: `/tmp/chewing-plus-voxkit.sock` (`VOXKIT_SOCKET`).
Model id: `qwen3-asr-0.6b`.

## Signing and notarization

`macos/sign.sh` signs every dylib in the app, then the app, then the helper
if you pass it.

- `APPLE_SIGN_ID` unset: `codesign --sign -` (ad-hoc). Fine for a local
  `~/Library/Input Methods` copy you built yourself. A zip from the internet
  will be rejected by Gatekeeper.
- `APPLE_SIGN_ID="Developer ID Application: Name (TEAMID)"`: hardened runtime
  and a timestamp.
- Also set `NOTARY_KEYCHAIN_PROFILE` to a profile created with
  `xcrun notarytool store-credentials`. The script submits a zip and staples
  the ticket. Do not commit that credential.

Identifiers: `im.chewing.plus.inputmethod`, `im.chewing.plus.voxkit`.

The helper opens the microphone, so the TCC prompt is attributed to it.
`NSMicrophoneUsageDescription` is on both Info.plists. Sandboxed builds need
the entitlement `com.apple.security.device.audio-input`. This tree does not
sandbox the processes. The first `cpal` open on macOS raises the system
microphone prompt.

## CI

`.github/workflows/macos.yml` is separate from `check.yml`, and every job
sets `continue-on-error`. `macos-15` and `macos-latest` run
`macos/typecheck.sh`. `macos-latest` also runs `macos/build-app.sh`, which
produced `ChewingPlus.app` on the runner (libchewing 0.14 linked, `libc++`
required). `macos-14` is not offered for this account
(`macos-14-arm64-us-default` is not a supported configuration), so the
workflow does not request it. Linux `check.yml` is unchanged and stays the
required build.

## Manual test on your Mac

Do these in order. Stop at the first failure and note the exact message.

1. `xcode-select -p` prints a developer directory. `cargo --version` is
   1.85 or newer. `git lfs version` works.
2. `macos/build-voxkit.sh` prints a path ending in `target/release/voxkit`.
   `file` on that binary says Mach-O.
3. `macos/build-app.sh` prints `app: .../ChewingPlus.app`.
   `ChewingPlus.app/Contents/Resources/libchewing/static_dict.bin` exists.
   `ChewingPlus.app/Contents/Frameworks` contains `libchewing.dylib`.
   `otool -L ChewingPlus.app/Contents/MacOS/ChewingPlus` shows
   `@rpath/libchewing.dylib` or a Frameworks path, not a path under `/tmp`.
4. `APPLE_SIGN_ID= macos/sign.sh macos/build/ChewingPlus.app` (ad-hoc)
   exits 0. `codesign -dv macos/build/ChewingPlus.app` says `Signature=adhoc`.
5. `macos/install.sh` copies the app into `~/Library/Input Methods` and
   `launchctl print gui/$UID/im.chewing.plus.voxkit` shows the service.
   The plist on disk does not contain the text `__HOME__`.
6. Log out and back in, or reboot. System Settings → Keyboard → Input
   Sources → add 新酷音 Plus (Traditional Chinese). Switch to it in a
   TextEdit window.
7. Type `su3cl3` (standard bopomofo). The marked text becomes `你好`.
   Return commits `你好` and clears the marked text.
8. `export CHEWING_PLUS_FEATURES=emoji,calculator,space,shortcode,english`
   before launching, or set that in the app's environment, then:
   - empty field, `=1+2*3`, Return commits `7`
   - `;;thumbsup` then Space commits 👍
   - Ctrl+Shift+Space, type `github`, Space inserts `GitHub`
   - tap Right Shift twice: marked text shows `[EN]`
9. While composing, the candidate panel lists rows. Click one, or press
   1–9 with no modifiers when that many rows are visible. The chosen row
   is committed or inserted and the panel hides.
10. Focus a password field (Safari saved-password prompt, or Terminal
    `sudo`). Keystrokes must go to the field, not into a marked `你好`.
    Right Option must not start dictation. (Secure Event Input is on.)
11. `VOXKIT_MODELS_DIR="$HOME/Library/Application Support/ChewingPlus/models" \
    "$HOME/Library/Application Support/ChewingPlus/voxkit" model install qwen3-asr-0.6b`
    then `launchctl kickstart -k gui/$UID/im.chewing.plus.voxkit`.
    macOS asks for microphone access for the helper. Allow it.
    Hold Right Option, say a short Traditional Chinese phrase, release.
    The text is inserted at the caret. The recording never leaves the Mac.
12. `macos/uninstall.sh`. The input source disappears after the next login.
    Model files are still in Application Support.

## Still only verifiable on a Mac

- `swiftc` linking the `.app` (this environment is Linux).
- CoreAudio capture and the microphone prompt.
- IMKCandidates placement and secure-input fields.
- launchctl bootstrap.
- Developer ID notarization (no credential in CI).
