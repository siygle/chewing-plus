# Development

## Design

- Branch `plus` on top of upstream [fcitx/fcitx5-chewing](https://github.com/fcitx/fcitx5-chewing) (`master` tracks upstream).
- Uses **only the public libchewing C API**; libchewing itself is not patched.
- Requires libchewing **0.14** (new system data: `static_dict.bin`, `static_lm.bin`, …).
  The plugin locates system data via `static_dict.bin` first, then the old `tsi.dat`.
- Plus code lives in `src/plus/`:
  - `ChewingPlus.*`: panels (calculator, emoji shortcode, English), hint tracking, dictation insert
  - `PlusFeatures.*`, `PlusEmoji.*`, `PlusEnglish.*`, `PlusDoubleTap.h`: shared with fcitx5-mcbopomofo-plus
- All options default off and are disabled in password/sensitive fields.

### Engine limitations (why some features commit early)

- English goes **into** the Chewing buffer via `SYMBOL_MODE`, so surrounding Chinese stays editable.
- Emoji and dictated Chinese are not ASCII and cannot enter the Chewing buffer: they are committed
  together with the composition (`head + text + tail`) at the cursor.
- Single-key emoji prefixes reject `` ` `` (Chewing's symbol key) and keys that are Bopomofo on some
  layouts (`; , . / - ' = [ ] \`). The double-tap prefix (default `;;`) undoes the first tap.

### Optional dictation (fcitx5-voxkit)

When CMake finds `Fcitx5ModuleVoxkit` (`ENABLE_VOXKIT`, default = found), the engine registers an
insert handler and the user's own phrases as vocabulary. Works with or without the module at runtime
(`voxkit` is an optional addon dependency).

## Build

```bash
# libchewing 0.14 (expected next to this repo as ~/Work/libchewing)
cmake --preset rust-release -S ~/Work/libchewing -B ~/Work/libchewing/build -DBUILD_DOC=OFF
cmake --build ~/Work/libchewing/build
DESTDIR=/tmp/lc-stage cmake --install ~/Work/libchewing/build

export PKG_CONFIG_PATH=/tmp/lc-stage/usr/lib/pkgconfig PKG_CONFIG_SYSROOT_DIR=/tmp/lc-stage
cmake -B build -G Ninja && cmake --build build
```

## Tests

All tests run in an isolated HOME / XDG dirs, so the real `~/.local/share/chewing` is never read or migrated.

```bash
test/run-plus-test.sh build /tmp/lc-stage/usr/share/libchewing /tmp/lc-stage/usr/lib
# with dictation (needs a fcitx5-voxkit build next to this repo)
test/run-voxkit-test.sh build /tmp/lc-stage/usr/share/libchewing /tmp/lc-stage/usr/lib \
    ~/Work/fcitx5-voxkit/build ~/Work/fcitx5-voxkit
```

`run-plus-test.sh` also runs the upstream `testchewing` suite.

## Packaging (Arch)

```bash
(cd packaging/libchewing && makepkg -fd)      # libchewing-plus (0.14 from ~/Work/libchewing)
(cd packaging/fcitx5-chewing && makepkg -fd)  # fcitx5-chewing-plus (this branch)
```

Both conflict with the Arch `libchewing` / `fcitx5-chewing` packages and are swapped in one pacman
transaction by `packaging/ime-switch/ime-switch` (see its README).

## Data

- `data/plus-emoji-shortcodes.txt`: generated from GitHub gemoji (MIT, `data/LICENSE.gemoji`)
- `data/plus-english.txt`: generated from SCOWL/ESDB size 50 (MIT-like, `data/LICENSE.scowl`)

Generators live in the fcitx5-mcbopomofo-plus repo (`scripts/make-emoji-shortcodes.py`,
`scripts/make-english-words.py`).
