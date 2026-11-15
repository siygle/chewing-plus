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
answers `ping`, `start`, and `stop` with `final` `測試`.
