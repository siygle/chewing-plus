# Cross-platform chewing-plus

> **Status:** All three frontends (fcitx5, native IBus, and native macOS
> InputMethodKit) are implemented and call the same C ABI
> (`include/chewing_plus.h`) backed by `libchewing-plus-core`. All three also
> integrate with the local `voxkit` daemon via `docs/VOXKIT_PROTOCOL.md` v1.
> The design notes below document the architecture and historical voice-option
> evaluation.

## Architecture

```
                    libchewing 0.14  (bundled; see docs/DEVELOPMENT.md)
                              |
                    libchewing-plus-core
                    C++ Plus engine  +  C ABI (include/chewing_plus.h)
                    /            |            \
            fcitx5 addon      IBus engine     macOS IMK app
            (src/eim.cpp)     (src/ibus/)     (macos/ChewingPlus/)
```

`libchewing-plus-core` is a CMake static library (shared if
`-DCHEWING_PLUS_CORE_SHARED=ON`). It links libchewing and nothing from fcitx.
Sources:

| Piece | License | Role |
|---|---|---|
| `src/plus/ChewingPlus.*` | LGPL-2.1-or-later | panels, hints, dictation insert, commit post-processing |
| `src/plus/PlusFeatures.*` | MIT | calculator, auto spacing, curated emoji |
| `src/plus/PlusEmoji.*` | MIT | shortcode table |
| `src/plus/PlusEnglish.*` | MIT | English lexicon |
| `src/plus/PlusDoubleTap.h` | MIT | Right Shift (or any watched key) double tap |
| `src/abi/chewing_plus.cpp` | LGPL-2.1-or-later | C ABI session: owns one `ChewingContext` |
| `data/plus-emoji-shortcodes.txt` | gemoji, MIT | built-in shortcodes |
| `data/plus-english.txt` | SCOWL, MIT-like | built-in English words |

The C ABI is specified in [ABI.md](ABI.md). New frontends should use it and
not call libchewing themselves, so Plus policy stays in one place.

The fcitx5 addon (`src/eim.cpp`) still calls the C++ class directly and still
owns its `ChewingContext`. That keeps fcitx behavior on the code path that
already shipped. Moving that addon onto the C ABI is a later phase, after the
ABI tests have been the ones people trust.

Build only the core and its tests, with no fcitx packages installed:

```bash
cmake -B build-core -DCHEWING_PLUS_CORE_ONLY=ON -G Ninja
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

## What stays in each frontend

The core returns text. It does not draw.

| Concern | Core | fcitx5 (today) | IBus (later) | IMK (later) |
|---|---|---|---|---|
| Candidate UI | strings, labels, kind | `ChewingCandidateList`, horizontal/vertical, arrow cursor | IBus lookup table | `IMKCandidates` or an AppKit panel. No Qt, no shared UI toolkit |
| Preedit | UTF-8, caret, bopomofo span | client preedit vs. the fcitx preedit, underline / highlight | IBus preedit attributes | `setMarkedText` |
| Settings | setters only | fcitx config (the `Plus:` keys) | IBus config or a GSettings schema | `NSUserDefaults` / a SwiftUI settings window |
| Password field | `chewing_plus_set_sensitive` | `CapabilityFlag::PasswordOrSensitive` | `IBUS_INPUT_PURPOSE_PASSWORD` (and PIN) | do not attach the IM to `NSSecureTextField`; if a client still asks, set sensitive |
| Data paths | receives paths, does not search | `StandardPaths`: pkg data + `~/.local/share/fcitx5-chewing/` | XDG, e.g. `$XDG_DATA_HOME/ibus-chewing-plus/` and the libchewing data dir | `Application Support/ChewingPlus/` and a bundled `static_dict.bin` |
| Keyboard shortcuts | Ctrl+Shift+Space and Right Shift ×2 are recognized if the frontend sends those events | fcitx key lists (`PlusEnglishToggleKeys`, `PlusEnglishDoubleTapKeys`) | map the user's shortcuts onto the same events, or call `chewing_plus_toggle_english` | same, from `handleEvent` |
| Voice capture | none. `chewing_plus_insert_text` + `chewing_plus_user_phrases` | fcitx5-voxkit module (below) | not in this phase | not in this phase |

libchewing's user dictionary is per session (`chewing_user_path`). On a
machine with both fcitx and IBus, sharing one directory means sharing learned
phrases; using two directories does not. That choice is open (see below).

## fcitx5, unchanged on purpose

`src/eim.cpp` still:

- creates the `ChewingContext` (preferring a directory that contains
  `static_dict.bin`)
- applies the fcitx config, including every `Plus:` option
- routes keys, candidate paging, selection keys and the arrow-key cursor
- draws preedit, aux and the candidate window
- skips Plus when the client is password/sensitive, and skips auto spacing
  on those commits
- loads `data/chewing-plus-emoji-shortcodes.txt` and
  `data/chewing-plus-english.txt` from the fcitx data dirs, then the user's
  copies under `~/.local/share/fcitx5-chewing/`

The only build change is that those `src/plus` files are compiled into
`libchewing-plus-core` and the addon links that library. The linker pulls
only the object files the addon references, so `chewing_plus_*` is not
exported from the fcitx module. IBus and IMK link `libchewing-plus-core`
themselves.

## VOICE INPUT DESIGN

Voice is required in the first cross-platform release, but **this phase does
not implement it for IBus or macOS**. The owner picks one option per platform
first. What follows is the constraint those options have to meet, then two or
three choices each.

### What fcitx5 does today

Optional, compile-time `ENABLE_VOXKIT` (on when CMake finds
`Fcitx5ModuleVoxkit`). At runtime the module may still be absent; the addon
dependency is optional.

From `src/eim.cpp` and fcitx5-voxkit:

1. The user holds Right Alt (configurable, modifier only). A press shorter
   than 250 ms is ignored. Another key during the hold cancels.
2. The fcitx module watches that key **before** the input method and talks to
   the `voxkit` daemon over a Unix socket.
3. The daemon records the microphone, runs Silero VAD, recognizes with
   **Qwen3-ASR-0.6B** by default (Whisper is also wired in the same daemon),
   and converts to Taiwan Traditional Chinese with OpenCC `s2twp`. The model
   is loaded once at service start. Audio does not leave the machine.
4. If this addon registered an insert handler, the transcript is delivered
   there. Otherwise voxkit commits it only when the input method is idle.
5. `ChewingPlus::insertDictation` closes any Plus panel, drops a half-typed
   bopomofo syllable, then:
   - ASCII (English dictation) goes into the Chewing buffer via `SYMBOL_MODE`,
     so Chinese around the cursor stays editable
   - anything else is committed as `head + text + tail` at the cursor
6. Vocabulary sent back to the daemon is the user's own Chewing phrases,
   longest first, at most 80 phrases, each at least two characters. The
   daemon further caps the prompt (default 40 characters) because a long
   prompt is decoded on every run.
7. Password/sensitive clients never start dictation (the module's check) and
   this addon would also skip Plus on the way out.

Licenses on that stack: the voxkit module and daemon are MIT; Silero VAD is
MIT; OpenCC is Apache-2.0; Qwen3-ASR-0.6B is published by Alibaba under
Apache-2.0 (confirm the model card before redistributing the weights);
Whisper weights and whisper.cpp are MIT. The weights are not in this repo.

The socket messages are version 1 and are written down in
[VOXKIT_PROTOCOL.md](VOXKIT_PROTOCOL.md). fcitx5 keeps using the voxkit
module; that module is already a version-1 client. IBus and macOS speak the
same socket.

The socket messages are version 1. The normative text is
[VOXKIT_PROTOCOL.md](VOXKIT_PROTOCOL.md). The fcitx5 chewing addon does not
open the socket; the voxkit module already speaks version 1, and this tree
does not change that addon.

The C ABI already has the two hooks a non-fcitx frontend needs:
`chewing_plus_insert_text` (step 5) and `chewing_plus_user_phrases` (step 6).
Capture, the hotkey, the model, and the "listening" indicator stay in the
frontend. `chewing_plus_set_sensitive` makes `insert_text` a no-op.

### macOS

Prefer a native app. No Qt, no cross-platform widget toolkit. Push-to-talk
can be a modifier monitored while the IM is active (Right Option is the
closest match to Right Alt). Microphone and speech-recognition permission
prompts belong to the app.

**Option A — Speech framework (`SFSpeechRecognizer`)**

| | |
|---|---|
| License | Apple SDK. The recognizer is not an open-source component. |
| Privacy | On-device only when `requiresOnDeviceRecognition` is true **and** the zh-TW (or zh-Hant) assets are installed. If on-device is unavailable, the same API can send audio to Apple. The app must refuse to start, not silently fall back to the network, if the product promise is "audio stays on the machine". |
| Traditional Chinese | Usable for everyday Mandarin. Taiwan wording and names are weaker than Qwen3-ASR + OpenCC `s2twp`. `contextualStrings` can take a short hint from `chewing_plus_user_phrases`, with a small cap. |
| Effort | Smallest native path: permission, push-to-talk, `SFSpeechAudioBufferRecognitionRequest`, then `chewing_plus_insert_text`. No model to ship. Quality and the on-device gap are the risk. |

**Option B — whisper.cpp inside the app**

| | |
|---|---|
| License | whisper.cpp is MIT. Whisper model weights are MIT. OpenCC (Apache-2.0) is still needed to force Taiwan Traditional; Whisper often emits Simplified or a mix. |
| Privacy | Local. Audio and the model stay in the app container. |
| Traditional Chinese | Decent on `medium` / `large`, poor on the tiny models, and still behind a Mandarin-specific model for Taiwan usage, names, and Bopomofo-adjacent jargon. A short `prompt` can carry user phrases. |
| Effort | Medium. `AVAudioEngine` for the mic, whisper.cpp's C API on a background queue, an OpenCC pass, then `insert_text`. The model is hundreds of MB to over 1 GB and has to be downloaded or bundled. No shared code with the Linux daemon unless we later wrap it. |

**Option C — the voxkit daemon (Qwen3-ASR), as a launchd helper or in-process**

| | |
|---|---|
| License | Daemon code MIT. Qwen3-ASR-0.6B Apache-2.0 (confirm the model card). OpenCC Apache-2.0. Silero VAD MIT. transcribe.cpp is part of the voxkit daemon tree. |
| Privacy | Local, same promise as fcitx5 today. |
| Traditional Chinese | The same engine current Plus users already have, including OpenCC `s2twp` and the user-phrase prompt. This is the quality bar the other options are measured against. |
| Effort | Largest. The daemon is Rust and talks over a Unix socket; on macOS that becomes a launchd agent plus a small client in the IMK process, or the recognizer linked in-process. Model is about 850 MB (`voxkit model install qwen3-asr-0.6b`, or the Hex model dir the daemon already reuses). Code signing and the microphone entitlement are extra. Behavior then matches Linux, which is why it is on the list. |

### IBus

The IBus engine would be C (or C++ only as far as linking the core), using
this repo's C ABI. Candidate UI is an IBus lookup table. Password purpose
comes from the IBus input context. No Qt.

**Option A — talk to the existing voxkit daemon**

| | |
|---|---|
| License | Same as the fcitx stack (MIT daemon, Apache-2.0 model and OpenCC). |
| Privacy | Local. Same process boundary as fcitx: the daemon holds the mic and the model, the IBus engine only sees text. |
| Traditional Chinese | Identical to the fcitx5 addon, including OpenCC `s2twp` and the phrase prompt. |
| Effort | Medium, and only if the socket protocol is treated as a small stable client rather than copied out of the fcitx module. The engine would own the hotkey (IBus has no fcitx "before IM" filter), push audio control messages, and call `chewing_plus_insert_text` with `chewing_plus_user_phrases` as the prompt. One daemon can serve both fcitx and IBus on a machine that has both. The version-1 messages are specified in [VOXKIT_PROTOCOL.md](VOXKIT_PROTOCOL.md). |

**Option B — whisper.cpp in the IBus process (or a tiny sidecar)**

| | |
|---|---|
| License | MIT engine, MIT weights, Apache-2.0 OpenCC. |
| Privacy | Local. |
| Traditional Chinese | Same limitation as macOS option B: needs OpenCC `s2twp`, weaker on Taiwan Mandarin and names than Qwen3-ASR. |
| Effort | Medium. whisper.cpp has a C API. Capture is PulseAudio or PipeWire. No dependency on fcitx or on the voxkit socket. Model download and a background thread stay inside the IBus engine. This is the option that does not require the voxkit project at all. |

**Option C — link the voxkit recognizer in-process, without the fcitx module**

| | |
|---|---|
| License | Same as option A. |
| Privacy | Local. The model and the microphone live in the IBus process instead of a daemon. |
| Traditional Chinese | Same as option A, if the OpenCC pass and the prompt cap are kept. |
| Effort | Large. It re-embeds the Rust daemon (or transcribe.cpp) into an IBus engine, duplicates model lifetime that the daemon already solved, and loses the "one recognizer for every input method" property. Prefer option A if the socket client is acceptable; this option exists so IBus still has a Qwen3-ASR path when a daemon is not wanted. |

### Recommendation to decide, not to build

The core side is already fixed: transcripts enter through
`chewing_plus_insert_text`, prompts can be built from
`chewing_plus_user_phrases`, and password fields are
`chewing_plus_set_sensitive`. Left open:

- macOS: A (Speech, smallest, privacy and Taiwan quality need a hard on-device
  requirement) vs C (same engine as Linux, most work) vs B (local, middle,
  weaker zh-TW).
- IBus: A (reuse the daemon) vs B (whisper.cpp, no voxkit dependency).
- Whether the voxkit socket becomes a documented client or stays private to
  the fcitx module.

## Open questions

- Should the fcitx addon move onto the C ABI in a later phase, or keep the
  C++ calls indefinitely?
- Shared libchewing user directory across fcitx and IBus, or one each?
- Ship `libchewing-plus-core` as the static archive inside each frontend
  (the default) or as one shared library?
- macOS and IBus voice, from the options above.
- The packaged Arch `libchewing-plus` version string in
  `packaging/libchewing/PKGBUILD` tracks a local checkout described as
  `0.14.0-rc.1` plus later commits. This tree pins the tag `v0.14.0-rc.1`
  (`d1506842`). Say if the pin should move to that later commit instead.
