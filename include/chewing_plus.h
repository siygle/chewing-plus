/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Stable C ABI for libchewing-plus-core.
 *
 * This is the boundary for native frontends (fcitx5 today via the C++ API,
 * IBus and a macOS InputMethodKit app later). The layout of every struct and
 * the meaning of every function below is ABI version 1. Add functions to
 * extend it; do not change existing structs or return conventions.
 *
 * Ownership
 *   - A session is single-threaded. Do not share one across threads and do
 *     not re-enter the session from a callback.
 *   - Pointers returned by the session (preedit, aux, candidates, commit,
 *     panel input, user phrases aside) are UTF-8 and live only until the
 *     next call on that session. Copy them if you need to keep them.
 *   - Commit callback strings are valid only for the duration of the call.
 *   - chewing_plus_user_phrases() memory is owned by the caller and released
 *     with chewing_plus_free_strings().
 *   - NULL session pointers are ignored (no-ops / zero / NULL).
 *
 * What the core owns
 *   - one libchewing context and the Plus state machine (calculator, emoji
 *     shortcodes, English mode, hints, auto spacing, emoji candidates)
 *   - the password/sensitive switch: when set, Plus does not run and
 *     commits are not auto-spaced. Detecting a password field is the
 *     frontend's job.
 *
 * What the frontend owns
 *   - drawing candidates, preedit and aux
 *   - which candidate is highlighted (pass the chosen index to
 *     chewing_plus_select)
 *   - settings UI, data-file locations, and keyboard-shortcut mapping
 *   - voice capture. The core only accepts a finished transcript
 *     (chewing_plus_insert_text) and can list user phrases for a prompt.
 *
 * See docs/ABI.md for a longer walkthrough.
 */
#ifndef CHEWING_PLUS_H_
#define CHEWING_PLUS_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHEWING_PLUS_ABI_VERSION 1

/* libchewing release/commit this tree is built and tested against. */
#define CHEWING_PLUS_LIBCHEWING_PIN "0.14.0-rc.2"

#if defined(_WIN32)
#if defined(CHEWING_PLUS_BUILDING)
#define CHEWING_PLUS_API __declspec(dllexport)
#else
#define CHEWING_PLUS_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define CHEWING_PLUS_API __attribute__((visibility("default")))
#else
#define CHEWING_PLUS_API
#endif

typedef struct chewing_plus_session chewing_plus_session;

/* Non-character keys. Printable input is `ascii`, with key == NONE. */
enum {
    CHEWING_PLUS_KEY_NONE = 0,
    CHEWING_PLUS_KEY_BACKSPACE = 1,
    CHEWING_PLUS_KEY_ESCAPE = 2,
    CHEWING_PLUS_KEY_RETURN = 3, /* also KP_Enter; frontends map both here */
    CHEWING_PLUS_KEY_SPACE = 4,
    CHEWING_PLUS_KEY_TAB = 5,
    CHEWING_PLUS_KEY_DELETE = 6,
    CHEWING_PLUS_KEY_UP = 7,
    CHEWING_PLUS_KEY_DOWN = 8,
    CHEWING_PLUS_KEY_LEFT = 9,
    CHEWING_PLUS_KEY_RIGHT = 10,
    CHEWING_PLUS_KEY_PAGE_UP = 11,
    CHEWING_PLUS_KEY_PAGE_DOWN = 12,
    CHEWING_PLUS_KEY_HOME = 13,
    CHEWING_PLUS_KEY_END = 14,
    CHEWING_PLUS_KEY_SHIFT_L = 15,
    CHEWING_PLUS_KEY_SHIFT_R = 16
};

enum {
    CHEWING_PLUS_MOD_SHIFT = 1u << 0,
    CHEWING_PLUS_MOD_CTRL = 1u << 1,
    CHEWING_PLUS_MOD_ALT = 1u << 2,
    CHEWING_PLUS_MOD_RELEASE = 1u << 3
};

enum {
    CHEWING_PLUS_PANEL_NONE = 0,
    CHEWING_PLUS_PANEL_CALCULATOR = 1,
    CHEWING_PLUS_PANEL_EMOJI = 2,
    CHEWING_PLUS_PANEL_ENGLISH = 3
};

enum {
    CHEWING_PLUS_CAND_CHEWING = 0, /* a libchewing choice on the current page */
    CHEWING_PLUS_CAND_EMOJI = 1,   /* Plus emoji appended to that page */
    CHEWING_PLUS_CAND_PANEL = 2    /* calculator / shortcode / English panel */
};

/*
 * Zero the struct before use. `time_ms` is a monotonic millisecond timestamp.
 * Pass a negative value to let the session read its own clock (fine for
 * interactive use; tests should pass an explicit clock).
 *
 * `keycode` is an evdev keycode, or 0 if unknown. Codes 10..18 are the digit
 * keys 1..9 and select a Plus-panel candidate together with Ctrl (English)
 * or Shift (calculator / emoji), independent of the character `ascii` holds.
 * If keycode is 0, ascii '1'..'9' with the same modifier is used instead.
 *
 * Ctrl+Shift+Space toggles English mode when that feature is enabled.
 * Double-tapping CHEWING_PLUS_KEY_SHIFT_R does too, when the double-tap
 * option is on. Send both the press and the release.
 */
typedef struct chewing_plus_key_event {
    uint32_t key;
    uint32_t ascii;
    uint32_t mods;
    int32_t keycode;
    int64_t time_ms;
} chewing_plus_key_event;

/*
 * `commit` fires once per committed string, already auto-spaced unless the
 * session is sensitive. `updated` fires after preedit / candidates / aux /
 * panel may have changed. Either function may be NULL.
 */
typedef struct chewing_plus_callbacks {
    void (*commit)(void *user, const char *utf8);
    void (*updated)(void *user);
    void *user;
} chewing_plus_callbacks;

CHEWING_PLUS_API int chewing_plus_abi_version(void);
CHEWING_PLUS_API const char *chewing_plus_libchewing_pin(void);

/*
 * `chewing_sys_path` is the directory that contains static_dict.bin (libchewing
 * 0.14). NULL uses CHEWING_PATH, then the compile-time default.
 * `chewing_user_path` is the user-dictionary directory. NULL uses
 * CHEWING_USER_PATH, then the platform default (which can be the real
 * home directory — pass a private directory from tests and frontends).
 * Returns NULL if libchewing cannot create a context.
 */
CHEWING_PLUS_API chewing_plus_session *
chewing_plus_new(const char *chewing_sys_path, const char *chewing_user_path);
CHEWING_PLUS_API void chewing_plus_free(chewing_plus_session *session);

CHEWING_PLUS_API void
chewing_plus_set_callbacks(chewing_plus_session *session,
                           const chewing_plus_callbacks *callbacks);

/*
 * Text before the cursor in the application, for the English quick insert
 * (Shift Shift, Ctrl+Shift+Space): when Chinese precedes the cursor the core
 * puts a space before the English word, as it already does after it.
 *
 * The core calls `fn` only when it needs the answer and the Chewing buffer
 * has nothing before the cursor (otherwise it reads the buffer itself), and
 * never while the session is sensitive. `fn` writes up to `cap - 1` bytes of
 * UTF-8 ending at the cursor (the last few characters are enough; do not cut
 * a character in half), NUL-terminates, and returns the byte count. Return 0
 * when the client has no surrounding text or the position is unknown: the
 * core then adds nothing. Pass NULL to remove it. A frontend whose `user`
 * pointer can die before the session must remove it first.
 */
typedef int (*chewing_plus_preceding_text_fn)(void *user, char *buf,
                                              size_t cap);
CHEWING_PLUS_API void
chewing_plus_set_preceding_text(chewing_plus_session *session,
                                chewing_plus_preceding_text_fn fn, void *user);

/*
 * Plus data files. Built-in files load first; user files override.
 * NULL leaves that path unchanged. Pass "" to clear a path.
 * Enabling a feature reloads the files that are currently set.
 * The core does not search XDG or Application Support itself.
 */
CHEWING_PLUS_API void chewing_plus_set_data_paths(chewing_plus_session *session,
                                                  const char *emoji_builtin,
                                                  const char *emoji_user,
                                                  const char *english_builtin,
                                                  const char *english_user);

/*
 * Phrase suggestions (data/plus-phrases.txt). While typing, the core lists
 * longer phrases that start with the composed text (中華 -> 中華民國) and
 * phrases whose syllables start with the typed Bopomofo (ㄍㄒㄈㄘ ->
 * 恭喜發財). The list is separate from the candidate window: frontends show
 * it as they like and pick with chewing_plus_select_suggestion().
 * `path` NULL keeps the previous file. Off by default.
 */
CHEWING_PLUS_API void
chewing_plus_set_suggestions(chewing_plus_session *session, int enabled,
                             const char *path);
/* Character file (data/plus-chars.txt or the compiled plus-chars.bin) for
 * the "live_chars" setting: while a syllable is being typed, the suggestion
 * list holds the likely characters for it (ㄋ -> 你 年 您). */
CHEWING_PLUS_API void chewing_plus_set_chars_path(chewing_plus_session *session,
                                                  const char *path);
/* Phrase file for suggestions (plain text or the compiled .bin), used by the
 * "suggestions" setting. Reloads if suggestions are on. */
CHEWING_PLUS_API void
chewing_plus_set_phrases_path(chewing_plus_session *session, const char *path);
CHEWING_PLUS_API int
chewing_plus_suggestion_count(const chewing_plus_session *session);
CHEWING_PLUS_API const char *
chewing_plus_suggestion(const chewing_plus_session *session, int index);
/* Put suggestion `index` in place of the matched text (the buffer tail or
 * the typed initials) and commit the composition, like an emoji pick. */
CHEWING_PLUS_API void
chewing_plus_select_suggestion(chewing_plus_session *session, int index);

/* All Plus features default off, matching the fcitx5 addon. */
CHEWING_PLUS_API void chewing_plus_set_emoji(chewing_plus_session *session,
                                             int enabled);
CHEWING_PLUS_API void chewing_plus_set_calculator(chewing_plus_session *session,
                                                  int enabled);
CHEWING_PLUS_API void chewing_plus_set_spacing(chewing_plus_session *session,
                                               int letters, int digits);
/*
 * `prefix` is one punctuation mark ("`") or the same mark twice (";;").
 * NULL keeps the previous prefix. `double_tap_ms` <= 0 keeps the previous
 * window (default 300).
 */
CHEWING_PLUS_API void
chewing_plus_set_emoji_shortcode(chewing_plus_session *session, int enabled,
                                 const char *prefix, int double_tap_ms);
/*
 * `shortcut_label` is display text for the English hint
 * ("Control+Shift+Space"). NULL keeps the previous label.
 */
CHEWING_PLUS_API void chewing_plus_set_english(chewing_plus_session *session,
                                               int enabled, int hint,
                                               const char *shortcut_label);
/*
 * When the English hint is showing, the toggle shortcut (combo or double
 * tap) inserts the hinted word in place of the reading instead of opening
 * the [EN] panel. Off by default.
 */
CHEWING_PLUS_API void
chewing_plus_set_english_hint_commit(chewing_plus_session *session,
                                     int enabled);
/* Ctrl+Shift+Space toggles English mode (on by default when English is
 * enabled). Frontends with their own configurable shortcut turn it off and
 * call chewing_plus_toggle_english() themselves. */
CHEWING_PLUS_API void
chewing_plus_set_english_combo(chewing_plus_session *session, int enabled);
/* Right Shift tapped twice. `window_ms` <= 0 keeps the previous window. */
CHEWING_PLUS_API void
chewing_plus_set_english_double_tap(chewing_plus_session *session, int enabled,
                                    int window_ms);

/*
 * Password / sensitive field. Plus key handling, English toggle, double-tap,
 * hints, emoji candidates, dictation insert and auto spacing all turn off.
 * The frontend decides when a client is sensitive.
 */
CHEWING_PLUS_API void chewing_plus_set_sensitive(chewing_plus_session *session,
                                                 int enabled);
CHEWING_PLUS_API int
chewing_plus_sensitive(const chewing_plus_session *session);

/* libchewing engine settings. Defaults match a stock fcitx5-chewing config. */
CHEWING_PLUS_API void
chewing_plus_set_keyboard_layout(chewing_plus_session *session,
                                 const char *kb_name);
CHEWING_PLUS_API void
chewing_plus_set_selection_keys(chewing_plus_session *session,
                                const char *ten_keys);
CHEWING_PLUS_API void chewing_plus_set_page_size(chewing_plus_session *session,
                                                 int page_size);
CHEWING_PLUS_API void
chewing_plus_set_easy_symbol(chewing_plus_session *session, int enabled);
CHEWING_PLUS_API void
chewing_plus_set_space_as_selection(chewing_plus_session *session, int enabled);
CHEWING_PLUS_API void
chewing_plus_set_phrase_options(chewing_plus_session *session,
                                int add_phrase_forward, int choice_backward,
                                int auto_shift_cursor);

/* Returns 1 if the key was consumed, 0 if the frontend should keep it. */
CHEWING_PLUS_API int chewing_plus_key(chewing_plus_session *session,
                                      const chewing_plus_key_event *event);

CHEWING_PLUS_API int chewing_plus_toggle_english(chewing_plus_session *session);

/*
 * ASCII (英) mode. While on, chewing_plus_key() returns 0 for every key, so
 * the frontend passes keys to the application unchanged. With the lone-Shift
 * option, pressing and releasing CHEWING_PLUS_KEY_SHIFT_L with no other key
 * in between (within 500 ms) switches the mode; send both the press and the
 * release. Switching to ASCII commits the composition first. Off by default.
 */
CHEWING_PLUS_API void
chewing_plus_set_shift_toggle(chewing_plus_session *session, int enabled);
CHEWING_PLUS_API int
chewing_plus_ascii_mode(const chewing_plus_session *session);
CHEWING_PLUS_API void chewing_plus_set_ascii_mode(chewing_plus_session *session,
                                                  int enabled);

/* Next (1) or previous (0) candidate page, wrapping around. Returns 1 when
 * the list changed. */
CHEWING_PLUS_API int chewing_plus_page(chewing_plus_session *session, int next);

/* Leave the session empty, committing first as asked. Used when the user
 * switches input methods or the text field loses focus. */
enum {
    CHEWING_PLUS_FLUSH_CLEAR = 0,   /* drop everything */
    CHEWING_PLUS_FLUSH_PREEDIT = 1, /* commit the text as shown */
    CHEWING_PLUS_FLUSH_DEFAULT = 2  /* commit the default choice */
};
CHEWING_PLUS_API void chewing_plus_flush(chewing_plus_session *session,
                                         int mode);

/*
 * Voice (and any other outside text). ASCII is inserted into the Chewing
 * buffer; anything else is committed with the surrounding composition.
 * No-op while sensitive, or if `utf8` is NULL or empty.
 */
CHEWING_PLUS_API void chewing_plus_insert_text(chewing_plus_session *session,
                                               const char *utf8);

CHEWING_PLUS_API void chewing_plus_reset(chewing_plus_session *session);

/* Polling. Out-pointers may be NULL. Strings are never NULL ("" at worst)
 * except when `session` is NULL. */
CHEWING_PLUS_API const char *chewing_plus_preedit(chewing_plus_session *session,
                                                  size_t *cursor,
                                                  size_t *highlight_start,
                                                  size_t *highlight_length);
CHEWING_PLUS_API const char *chewing_plus_aux(chewing_plus_session *session);
CHEWING_PLUS_API int chewing_plus_panel(const chewing_plus_session *session);
CHEWING_PLUS_API const char *
chewing_plus_panel_input(const chewing_plus_session *session);

CHEWING_PLUS_API int
chewing_plus_candidate_count(const chewing_plus_session *session);
CHEWING_PLUS_API const char *
chewing_plus_candidate(const chewing_plus_session *session, int index);
CHEWING_PLUS_API const char *
chewing_plus_candidate_label(const chewing_plus_session *session, int index);
CHEWING_PLUS_API int
chewing_plus_candidate_kind(const chewing_plus_session *session, int index);
CHEWING_PLUS_API void chewing_plus_select(chewing_plus_session *session,
                                          int index);

/*
 * Screen state in one call. Every field a frontend draws: preedit and its
 * highlight, aux line, open panel, candidate list and phrase suggestions.
 * `revision` changes whenever any of them changes, so a frontend redraws
 * only when it differs from the last one it drew (each redraw is a round
 * trip to the app on macOS and IBus). Pointers stay valid until the next
 * call on this session. Returns 0 if `session` or `out` is NULL.
 */
typedef struct chewing_plus_view {
    uint64_t revision;
    const char *preedit;
    size_t cursor;          /* byte offset in preedit */
    size_t highlight_start; /* byte range of the reading being typed */
    size_t highlight_length;
    const char *aux; /* hint line ("" when none) */
    int panel;       /* CHEWING_PLUS_PANEL_* */
    int candidate_count;
    int suggestion_count;
} chewing_plus_view;

CHEWING_PLUS_API int chewing_plus_get_view(chewing_plus_session *session,
                                           chewing_plus_view *out);

/*
 * Settings, one table for every frontend (src/abi/settings.inc). A frontend
 * enumerates them to build its settings UI and storage, and applies stored
 * values by key. Unknown keys and unparsable values are ignored (returns 0),
 * so a newer config file never breaks an older build.
 */
enum {
    CHEWING_PLUS_SETTING_BOOL = 0,
    CHEWING_PLUS_SETTING_INT = 1,
    CHEWING_PLUS_SETTING_STRING = 2
};

typedef struct chewing_plus_setting_info {
    const char *key;           /* "emoji" */
    int type;                  /* CHEWING_PLUS_SETTING_* */
    const char *default_value; /* "0", "300", ";;" */
    const char *label_en;
    const char *label_zh; /* Traditional Chinese */
} chewing_plus_setting_info;

CHEWING_PLUS_API int chewing_plus_setting_count(void);
/* Returns 0 if `index` is out of range. Strings are static. */
CHEWING_PLUS_API int
chewing_plus_setting_info_at(int index, chewing_plus_setting_info *out);
/* Booleans accept 1/0, true/false, on/off, yes/no (any case). */
CHEWING_PLUS_API int chewing_plus_set_setting(chewing_plus_session *session,
                                              const char *key,
                                              const char *value);
/* Value last set through chewing_plus_set_setting(), or the default; the
 * older per-feature setters do not update it. NULL for an unknown key.
 * Valid until the next call on this session. */
CHEWING_PLUS_API const char *
chewing_plus_get_setting(chewing_plus_session *session, const char *key);

/*
 * Text committed since the previous take, concatenated. Empty string if
 * nothing was committed. Also delivered live through the commit callback.
 */
CHEWING_PLUS_API const char *
chewing_plus_take_commit(chewing_plus_session *session);

/*
 * User phrases, longest first, capped, for a voice-recognition prompt.
 * NULL-terminated array; NULL if there are none. Each entry is at least
 * two Unicode scalar values. Caller frees with chewing_plus_free_strings().
 */
CHEWING_PLUS_API char **chewing_plus_user_phrases(chewing_plus_session *session,
                                                  size_t *count);
CHEWING_PLUS_API void chewing_plus_free_strings(char **list);

#ifdef __cplusplus
}
#endif

#endif /* CHEWING_PLUS_H_ */
