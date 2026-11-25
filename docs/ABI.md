# C ABI (`chewing_plus.h`)

`libchewing-plus-core` exposes a stable C API so an IBus engine (C) and a
macOS InputMethodKit app (Swift) can drive the same Plus engine the fcitx5
addon uses. The header is `include/chewing_plus.h`. ABI version is
`CHEWING_PLUS_ABI_VERSION` (currently 1). Add functions to extend it. Do not
reorder or resize `chewing_plus_key_event` or `chewing_plus_callbacks`.

All three frontends call this ABI: the fcitx5 addon (`src/eim.cpp`), the
IBus engine (`src/ibus/engine.c`) and the macOS input method. Each one only
maps its key events in and draws `chewing_plus_get_view()` out.

## Settings

One table, `src/abi/settings.inc`, lists every setting with its key, type,
default and labels. Frontends enumerate it with
`chewing_plus_setting_count()` / `chewing_plus_setting_info_at()` (the IBus
setup window and the macOS input menu build their switches from it) and
apply stored values with `chewing_plus_set_setting(session, key, value)`.
Unknown keys and bad values are ignored, so a newer config file never breaks
an older build. fcitx5 keeps its own option names in `chewing.conf` and
maps them onto these keys.

## Data files

Dictionaries are shared by every session in a process (`DataCache`). The
phrase index ships compiled: `chewing-plus-compile-phrases` turns
`data/plus-phrases.txt` into `plus-phrases.bin` at build time, and the core
memory-maps it (no parsing at load, pages shared between processes). The
text file still loads if passed instead.

## Session

```c
chewing_plus_session *chewing_plus_new(const char *chewing_sys_path,
                                       const char *chewing_user_path);
void chewing_plus_free(chewing_plus_session *session);
```

- `chewing_sys_path` is the directory that contains libchewing 0.14's
  `static_dict.bin`. NULL falls through to `CHEWING_PATH`, then libchewing's
  compiled-in default.
- `chewing_user_path` is the user-dictionary **directory**. NULL falls through
  to `CHEWING_USER_PATH`, then the platform default under the home directory.
  Tests and frontends should pass a private directory.
- One session is one libchewing context plus Plus state. It is not
  thread-safe. Callbacks must not call back into the session.

Engine defaults match a stock fcitx5-chewing config: page size 10, selection
keys `1234567890`, space-as-selection on, phrase added forward, backward
phrase choice, no auto-shift, Escape clears the buffer, Chinese mode, maximum
preedit length 18. Override them with the `chewing_plus_set_*` engine setters.
Plus features default **off**.

## Keys in

```c
int chewing_plus_key(chewing_plus_session *session,
                     const chewing_plus_key_event *event);
```

Zero the event. Returns 1 when the session consumed the key.

| Field | Meaning |
|---|---|
| `key` | `CHEWING_PLUS_KEY_*`. `NONE` when the key is a character in `ascii`. |
| `ascii` | Unicode scalar of a printable character, or 0. ASCII is `< 128`. |
| `mods` | `CHEWING_PLUS_MOD_SHIFT`, `CTRL`, `ALT`, `RELEASE`. |
| `keycode` | evdev keycode, or 0. 10..18 are the digit keys 1..9. |
| `time_ms` | monotonic milliseconds. Negative: the session reads its own clock. |

Send the press and the matching release. Releases are not fed to libchewing.
Right Shift tapped twice (press, release, press, release) toggles English mode
when `chewing_plus_set_english_double_tap` is on. Ctrl+Shift+Space toggles it
when English mode is enabled. Map the platform's keypad Enter to
`CHEWING_PLUS_KEY_RETURN`.

Plus panels are modal. Digit selection uses Ctrl+1..9 in English mode and
Shift+1..9 in the calculator and emoji panels. Pass the unmodified digit in
`ascii`, or the evdev keycode; do not pass the shifted glyph (`!` instead of
`1`) if you want it to select.

Arrow keys move the libchewing cursor / candidate page when no Plus panel is
open. The highlighted row inside a candidate popup is **frontend** state:
call `chewing_plus_select(session, index)` with the row the user confirmed.

## Polling out

Strings are UTF-8, never NULL for a live session (`""` if empty), and valid
only until the next call on that session.

| Call | Returns |
|---|---|
| `chewing_plus_preedit` | full preedit, caret byte offset, bopomofo highlight span |
| `chewing_plus_aux` | libchewing aux, else the English hint |
| `chewing_plus_panel` / `chewing_plus_panel_input` | `NONE`, `CALCULATOR`, `EMOJI`, `ENGLISH`, and the text typed into it |
| `chewing_plus_candidate*` | current page: chewing choices, an emoji row inserted at position 2 when Plus offers one, or the panel's own rows (at most 9). The emoji row is also offered when libchewing's own candidate window is closed, which is what the fcitx addon does. While that row is the whole list, Up/Down/Left/Right/Page are not forwarded to libchewing (they would open phrase selection, clamp the cursor, and drop the row). The highlighted index stays in the frontend; `chewing_plus_select` commits it. |
| `chewing_plus_take_commit` | text committed since the previous take, concatenated |

`chewing_plus_set_callbacks` can also deliver each commit as it happens
(`commit`) and a nudge to poll again (`updated`). Commit pointers passed to
the callback are valid only for that call. Auto spacing has already been
applied, unless the session is sensitive.

## Config and data

The core does not look up XDG paths or `Application Support`. The frontend
finds files and passes them in:

```c
chewing_plus_set_data_paths(session,
    emoji_builtin, emoji_user,
    english_builtin, english_user);
```

NULL leaves a path unchanged. `""` clears it. Built-in files load first; the
user file overrides. Turning a feature on reloads the files currently set.

| Setter | Feature |
|---|---|
| `chewing_plus_set_emoji` | emoji row on the candidate page |
| `chewing_plus_set_calculator` | `=` on an empty buffer |
| `chewing_plus_set_spacing` | spaces between Han and letters / digits |
| `chewing_plus_set_emoji_shortcode` | prefix (`;;` by default) and double-tap window |
| `chewing_plus_set_english` | English buffer, hint, shortcut label shown in the hint |
| `chewing_plus_set_english_double_tap` | Right Shift ×2 |
| `chewing_plus_set_sensitive` | password field; see below |

## Sensitive / password fields

`chewing_plus_set_sensitive(session, 1)` is the disabled state:

- Plus does not see keys (no calculator, shortcode, English, hint, emoji row)
- an open Plus panel is closed
- Right Shift double-tap and `chewing_plus_toggle_english` do nothing
- `chewing_plus_insert_text` does nothing
- commits are not auto-spaced

Detecting the field stays in the frontend (fcitx `PasswordOrSensitive`, IBus
`IBUS_INPUT_PURPOSE_PASSWORD`, a macOS secure field that never attaches the
IM). The frontend should also reset the session when the client changes, the
same way fcitx does.

## Voice hook

This ABI does not record audio. A frontend that already has a transcript calls:

```c
void chewing_plus_insert_text(chewing_plus_session *session, const char *utf8);
```

ASCII is typed into the Chewing buffer (`SYMBOL_MODE`) so the surrounding
Chinese stays editable. Anything else is committed as `head + text + tail`
and a half-typed bopomofo syllable is dropped. This is the same policy as
`ChewingPlus::insertDictation` in the fcitx addon.

```c
char **chewing_plus_user_phrases(chewing_plus_session *session, size_t *count);
void chewing_plus_free_strings(char **list);
```

returns the user's own phrases, longest first, capped at 80, each at least
two Unicode scalars. That is the list the fcitx addon hands to voxkit. The
caller frees it. Voice options themselves are in
[CROSS_PLATFORM.md](CROSS_PLATFORM.md); nothing here starts a recognizer.

## Licenses

The C ABI and `ChewingPlus` are LGPL-2.1-or-later. `PlusFeatures`,
`PlusEmoji`, `PlusEnglish` and `PlusDoubleTap` are MIT. The gemoji shortcodes
and the SCOWL word list keep the notices in `data/LICENSE.gemoji` and
`data/LICENSE.scowl`. libchewing is LGPL-2.1-or-later; the pin is
`CHEWING_PLUS_LIBCHEWING_PIN` (`0.14.0-rc.2`).
