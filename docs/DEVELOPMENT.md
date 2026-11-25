# Development

## Design

- Branch `plus` on top of upstream [fcitx/fcitx5-chewing](https://github.com/fcitx/fcitx5-chewing) (`master` tracks upstream).
- Uses **only the public libchewing C API**; libchewing itself is not patched.
- Requires libchewing **0.14** (new system data: `static_dict.bin`, `static_lm.bin`, …).
  The plugin locates system data via `static_dict.bin` first, then the old `tsi.dat`.
- Plus code lives in `libchewing-plus-core` (`src/plus/`, no fcitx headers):
  - `ChewingPlus.*`: panels (calculator, emoji shortcode, English), hint tracking, dictation insert
  - `PlusFeatures.*`, `PlusEmoji.*`, `PlusEnglish.*`, `PlusDoubleTap.h`: shared with fcitx5-mcbopomofo-plus
  - `src/abi/chewing_plus.cpp` and `include/chewing_plus.h`: stable C ABI for later IBus / macOS frontends ([ABI.md](ABI.md))
- The fcitx5 addon links that library and still calls the C++ class. Behavior for fcitx users is unchanged.
- All options default off and are disabled in password/sensitive fields.
- Cross-platform plan, including voice options that are **not** implemented yet: [CROSS_PLATFORM.md](CROSS_PLATFORM.md).

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

## libchewing pin

This tree is built and tested against **libchewing 0.14**
(`v0.14.0-rc.2`, commit `1bfcde9078f13b4d97aaa4b6a01cbd67687522e3`) from
<https://codeberg.org/chewing/libchewing>. The same pin is in
`cmake/ChewingPlusLibchewing.cmake` and `CHEWING_PLUS_LIBCHEWING_PIN`.
Building that tag needs **Rust 1.95 or newer** (Cargo edition 2024), CMake
3.27 or newer, and **git-lfs** (the dictionary sources are a submodule, and
the language-model files are LFS objects). `scripts/bundle-libchewing.sh`
clones the tag, checks out the submodule, and runs `git lfs pull`.

`scripts/bundle-libchewing.sh` clones that commit and installs it:

```bash
# shared library (default, and what CI tests). Prefix defaults to
# third_party/dist: a vendored libchewing.so plus its data files.
bash scripts/bundle-libchewing.sh --shared "$HOME/opt/libchewing-0.14"

# static archive. Also copies libchewing_capi.a (the Rust staticlib that the
# shared library absorbs and a static libchewing.a does not) into the prefix.
bash scripts/bundle-libchewing.sh --static "$HOME/opt/libchewing-0.14-static"
```

`--static` is wired through, but **libchewing 0.14.0-rc.2's own CMake refuses
to generate a static build**:

```
install(EXPORT "libchewingTargets" ...) includes target "libchewing" which
requires target "chewing_capi" that is not in any export set.
```

This repo does not patch libchewing. Use `--shared` until that export is
fixed upstream. `-DCHEWING_PLUS_LIBCHEWING_STATIC=ON` links `libchewing.a`
plus `libchewing_capi.a`, `dl`, `m` and `pthread` when a prefix actually
contains those archives.

Point a shared prefix at the build:

```bash
cmake -B build -G Ninja \
  -DCHEWING_PLUS_BUNDLE_LIBCHEWING=ON \
  -DCHEWING_PLUS_LIBCHEWING_PREFIX="$HOME/opt/libchewing-0.14"
cmake --build build
```

`--shared` is the configuration CI uses (it installs into `/usr` in the
container).

A checkout you already have (for example `~/Work/libchewing`) still works.
Install it and export `PKG_CONFIG_PATH` as below, or pass it as
`CHEWING_PLUS_LIBCHEWING_SRC` so the script will not clone. The script refuses
a checkout that is not the pinned commit.

```bash
# equivalent manual build of a local checkout.
# data/ is the libchewing-data submodule; the model files are Git LFS.
git lfs install --skip-repo
git -C ~/Work/libchewing submodule update --init --depth 1
git -C ~/Work/libchewing/data lfs install --local
GIT_LFS_SKIP_SMUDGE= git -C ~/Work/libchewing/data lfs pull
cmake -S ~/Work/libchewing -B ~/Work/libchewing/build -G Ninja \
  -DBUILD_DOC=OFF -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=/tmp/lc-stage
cmake --build ~/Work/libchewing/build
cmake --install ~/Work/libchewing/build

export PKG_CONFIG_PATH=/tmp/lc-stage/lib/pkgconfig
cmake -B build -G Ninja && cmake --build build
```

Core and the C ABI tests do not need fcitx:

```bash
cmake -B build-core -G Ninja -DCHEWING_PLUS_CORE_ONLY=ON \
  -DCHEWING_PLUS_BUNDLE_LIBCHEWING=ON \
  -DCHEWING_PLUS_LIBCHEWING_PREFIX="$HOME/opt/libchewing-0.14"
cmake --build build-core
CHEWING_TEST_DATA="$HOME/opt/libchewing-0.14/share/libchewing" \
  ctest --test-dir build-core --output-on-failure
```

`-DCHEWING_PLUS_CORE_SHARED=ON` builds `libchewing-plus-core.so` instead of
the default static archive. The fcitx addon links the static archive, so it
does not gain a new runtime dependency.

## Build

```bash
export PKG_CONFIG_PATH=/tmp/lc-stage/lib/pkgconfig   # or the prefix above
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

`plus-core` and `plus-abi` (under `test/core/`) do not link fcitx. They need
libchewing 0.14 data: set `CHEWING_TEST_DATA` to the directory that contains
`static_dict.bin`, or install that directory at `/usr/share/libchewing`.
`test/run-plus-test.sh` and `test/run-voxkit-test.sh` export
`CHEWING_TEST_DATA` from the data directory you pass in. CI sets it to
`/usr/share/libchewing` after `bash scripts/bundle-libchewing.sh`.

fcitx5's config dialog can store the active voice model id (`VoiceModel`)
and shows daemon, download state, store path, and disk use in
`VoiceModelStatus` when the dialog opens. It cannot draw a progress bar.
`chewing-plus-models` does list, download, cancel, and delete. A richer
fcitx page would be a custom widget inside fcitx5-configtool (Qt, the
toolkit that program already uses) or a D-Bus page. Neither is in this
tree.
`plus-abi` passes its own temporary user-dictionary directory into
`chewing_plus_new`. `testvoxkit` is built only when `ENABLE_VOXKIT` found
`Fcitx5ModuleVoxkit`.

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
