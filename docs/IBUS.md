# IBus frontend

Native IBus engine for `libchewing-plus-core`. It is not a Qt or GTK widget
set; it is the C engine `ibus-chewing-plus` plus a component XML file. Build
it with `-DENABLE_IBUS=ON` (needs `ibus-1.0` from the system). The fcitx5
addon is unchanged and still the default build.

```bash
cmake -B build -DENABLE_IBUS=ON -DCHEWING_PLUS_CORE_ONLY=ON
cmake --build build
sudo cmake --install build
ibus write-cache
ibus restart
```

The component is `org.freedesktop.IBus.ChewingPlus`, engine name
`chewing-plus`, language `zh_TW`. The push-to-talk key is `voice_key` in
`~/.config/chewing-plus/ibus.conf` (default `Alt_R`; the setup window has
a dropdown). It speaks
[VOXKIT_PROTOCOL.md](VOXKIT_PROTOCOL.md) version 1 to the existing voxkit
daemon (`voxkit serve`). The daemon owns the microphone and the model. If it
is not listening, the auxiliary text says so and normal Zhuyin still works.
A press shorter than 250 ms is cancelled instead of transcribed, and a key
typed while recording cancels the recording and is still typed.

With `voice_translate=1` in `~/.config/chewing-plus/ibus.conf` (the setup
window's 「語音翻譯」 switch), `voice_translate_key` (default `Control_R`,
same dropdown as the dictation key) is push-to-translate: the engine
checks once per connection that `model.status` lists the `translate`
capability, then sends `start` with `mode=translate` and
`target=voice_translate_to` (default `en`). A fallback final (no plugin,
model still downloading, ...) inserts the original text and shows the reason
in the auxiliary text. The translate key is an ordinary modifier while the
setting is off. If the two keys are the same, dictation wins and the setup
window warns. `Shift_L` / `Shift_R` are not accepted (Right Shift is the
English-mode double tap).

Plus features stay off unless `CHEWING_PLUS_IBUS_FEATURES` lists them
(`emoji`, `calculator`, `space`, `shortcode`, `english`), matching the fcitx5
defaults. Password and PIN purposes set the core's sensitive flag, which
disables Plus and refuses to start dictation.

Data files installed for this engine:

- `$prefix/share/chewing-plus/plus-emoji-shortcodes.txt`
- `$prefix/share/chewing-plus/plus-english.txt`

Override with `CHEWING_PLUS_EMOJI`, `CHEWING_PLUS_ENGLISH`, `CHEWING_PATH`,
`CHEWING_USER_PATH`, and `VOXKIT_SOCKET`.

Headless tests (no `ibus-daemon`, no microphone): `ibus-keys` and
`voxkit-client`. The second one is a fake daemon on a Unix socket that
answers `ping`, `start`, `stop`, `model.status`, and `model.list`.

## Setup program

`-DENABLE_IBUS_SETUP=ON` builds `ibus-setup-chewing-plus` and writes its
path into the component `<setup>` entry. It needs `libadwaita-1` (GTK4).
The engine does not link libadwaita. The window lists models, shows
download progress, and can cancel, delete, select, and show disk use and
daemon status. Those calls are the shared C client. Plus toggles
(emoji, calculator, spacing, shortcode, English) are written to
`$XDG_CONFIG_HOME/chewing-plus/ibus.conf`. The engine reads that file on
focus. The voice section also has dropdowns for `voice_key` (default
`Alt_R`) and `voice_translate_key` (default `Control_R`); values are X11
keysym names, the same ones fcitx5 writes to `conf/voxkit.conf`.
`CHEWING_PLUS_IBUS_FEATURES` still overrides the file when it is
set. `chewing-plus-models` remains for a terminal.

```bash
cmake -B build -DENABLE_IBUS=ON -DENABLE_IBUS_SETUP=ON \
  -DCHEWING_PLUS_CORE_ONLY=ON
cmake --build build
```

fcitx5 keeps its config form and the same CLI. There is no Qt page.
