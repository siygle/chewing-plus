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
`chewing-plus`, language `zh_TW`. Right Alt is push-to-talk. It speaks
[VOXKIT_PROTOCOL.md](VOXKIT_PROTOCOL.md) version 1 to the existing voxkit
daemon (`voxkit serve`). The daemon owns the microphone and the model. If it
is not listening, the auxiliary text says so and normal Zhuyin still works.

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
focus. `CHEWING_PLUS_IBUS_FEATURES` still overrides the file when it is
set. `chewing-plus-models` remains for a terminal.

```bash
cmake -B build -DENABLE_IBUS=ON -DENABLE_IBUS_SETUP=ON \
  -DCHEWING_PLUS_CORE_ONLY=ON
cmake --build build
```

fcitx5 keeps its config form and the same CLI. There is no Qt page.

## Appearance

The candidate window, preedit (when it is not embedded in the client) and
auxiliary text (English hints, phrase suggestions, voice status such as
「🎙 聆聽中」) are drawn by the IBus panel, not by this engine. There is no
font-size setting in `ibus.conf` or the setup window. To enlarge them, open
IBus Preferences → General → Font and Style, enable “Use custom font”, and
pick a larger face (the custom-font default is `Sans 10`; with the option
off, the panel uses the GTK font). Or:

```bash
gsettings set org.freedesktop.ibus.panel use-custom-font true
gsettings set org.freedesktop.ibus.panel custom-font 'Sans 13'
ibus restart
```

Schema: `org.freedesktop.ibus.panel` (`use-custom-font`, `custom-font`).
