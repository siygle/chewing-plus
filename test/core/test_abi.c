/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * C ABI tests. Linked as C against libchewing-plus-core; no fcitx.
 */
#include "chewing_plus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures = 0;
static chewing_plus_session *session = NULL;
static int64_t clock_ms = 1000;
static char cb_buf[2048];

static void check(int ok, const char *what) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

static void on_commit(void *user, const char *utf8) {
    (void)user;
    if (!utf8) {
        return;
    }
    strncat(cb_buf, utf8, sizeof(cb_buf) - strlen(cb_buf) - 1);
}

static void tap(uint32_t key, uint32_t ascii, uint32_t mods) {
    chewing_plus_key_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.key = key;
    ev.ascii = ascii;
    ev.mods = mods;
    ev.time_ms = clock_ms;
    clock_ms += 20;
    chewing_plus_key(session, &ev);
    ev.mods = mods | CHEWING_PLUS_MOD_RELEASE;
    ev.time_ms = clock_ms;
    clock_ms += 20;
    chewing_plus_key(session, &ev);
}

static void type_text(const char *text) {
    for (; text && *text; ++text) {
        unsigned char c = (unsigned char)*text;
        if (c == ' ') {
            tap(CHEWING_PLUS_KEY_SPACE, ' ', 0);
        } else {
            tap(CHEWING_PLUS_KEY_NONE, c,
                (c >= 'A' && c <= 'Z') ? CHEWING_PLUS_MOD_SHIFT : 0);
        }
    }
}

static const char *preedit(void) {
    const char *text = chewing_plus_preedit(session, NULL, NULL, NULL);
    return text ? text : "";
}

static const char *aux(void) {
    const char *text = chewing_plus_aux(session);
    return text ? text : "";
}

static void reset(void) {
    chewing_plus_reset(session);
    chewing_plus_take_commit(session);
    cb_buf[0] = '\0';
    clock_ms += 1000;
}

static int cand_has(const char *want) {
    int n = chewing_plus_candidate_count(session);
    int i;
    for (i = 0; i < n; i++) {
        const char *text = chewing_plus_candidate(session, i);
        if (text && strcmp(text, want) == 0) {
            return 1;
        }
    }
    return 0;
}

static void dump_cands(void) {
    int n = chewing_plus_candidate_count(session);
    int i;
    fprintf(stderr, "  preedit=[%s] aux=[%s] cands=%d\n", preedit(), aux(), n);
    for (i = 0; i < n && i < 8; i++) {
        fprintf(stderr, "  cand[%d] kind=%d [%s]\n", i,
                chewing_plus_candidate_kind(session, i),
                chewing_plus_candidate(session, i));
    }
}

static const char *find_sys_path(void) {
    static char buf[1024];
    const char *env = getenv("CHEWING_TEST_DATA");
    const char *candidates[4];
    size_t i;
    candidates[0] = (env && env[0]) ? env : NULL;
    candidates[1] = getenv("CHEWING_PATH");
    candidates[2] = "/usr/share/libchewing";
    candidates[3] = "/usr/local/share/libchewing";
    for (i = 0; i < 4; i++) {
        if (!candidates[i] || !candidates[i][0]) {
            continue;
        }
        snprintf(buf, sizeof(buf), "%s/static_dict.bin", candidates[i]);
        if (access(buf, R_OK) == 0) {
            return candidates[i];
        }
    }
    return NULL;
}

static void test_calculator(void) {
    reset();
    type_text("=1+2*3");
    check(strcmp(preedit(), "[計算器] =1+2*3") == 0, "calculator preedit");
    check(chewing_plus_panel(session) == CHEWING_PLUS_PANEL_CALCULATOR,
          "calculator panel");
    check(chewing_plus_candidate_count(session) >= 1 &&
              strcmp(chewing_plus_candidate(session, 0), "7") == 0,
          "calculator result 7");
    if (strcmp(preedit(), "[計算器] =1+2*3") != 0) {
        dump_cands();
    }
    tap(CHEWING_PLUS_KEY_RETURN, 0, 0);
    check(strcmp(chewing_plus_take_commit(session), "7") == 0,
          "calculator commits 7");
    check(strcmp(cb_buf, "7") == 0, "commit callback sees 7");
}

static void test_spacing(void) {
    reset();
    type_text("su3cl3");
    check(strcmp(preedit(), "你好") == 0, "baseline 你好");
    tap(CHEWING_PLUS_KEY_NONE, 'A', CHEWING_PLUS_MOD_SHIFT);
    tap(CHEWING_PLUS_KEY_RETURN, 0, 0);
    {
        const char *got = chewing_plus_take_commit(session);
        check(strcmp(got, "你好 A") == 0, "auto space 你好 A");
        if (strcmp(got, "你好 A") != 0) {
            fprintf(stderr, "  commit=[%s]\n", got);
        }
    }
}

static void test_shortcode(void) {
    reset();
    type_text("su3cl3");
    type_text(";;");
    check(strcmp(preedit(), "你好;;") == 0, ";; keeps 你好");
    if (strcmp(preedit(), "你好;;") != 0) {
        dump_cands();
    }
    type_text("thumbsup");
    check(chewing_plus_candidate_count(session) >= 1 &&
              strcmp(chewing_plus_candidate(session, 0), "👍 :thumbsup") == 0,
          "thumbsup candidate");
    tap(CHEWING_PLUS_KEY_SPACE, ' ', 0);
    {
        const char *got = chewing_plus_take_commit(session);
        check(strcmp(got, "你好👍") == 0, "shortcode commits 你好👍");
        if (strcmp(got, "你好👍") != 0) {
            fprintf(stderr, "  commit=[%s] preedit=[%s]\n", got, preedit());
        }
    }
    reset();
    type_text("w;");
    check(strcmp(preedit(), "ㄊㄤ") == 0, "single ; stays bopomofo");
    if (strcmp(preedit(), "ㄊㄤ") != 0) {
        dump_cands();
    }
}

static void test_english(void) {
    reset();
    type_text("su3cl3");
    check(chewing_plus_toggle_english(session) == 1, "toggle English");
    check(strstr(preedit(), "[EN]") != NULL, "EN marker keeps 你好");
    type_text("github");
    check(chewing_plus_candidate_count(session) >= 1 &&
              strcmp(chewing_plus_candidate(session, 0), "GitHub") == 0,
          "github -> GitHub");
    tap(CHEWING_PLUS_KEY_SPACE, ' ', 0);
    check(strstr(preedit(), "GitHub") != NULL, "English inserted into buffer");
    tap(CHEWING_PLUS_KEY_RETURN, 0, 0);
    {
        const char *got = chewing_plus_take_commit(session);
        check(strcmp(got, "你好 GitHub ") == 0, "English commit with spacing");
        if (strcmp(got, "你好 GitHub ") != 0) {
            fprintf(stderr, "  commit=[%s] preedit=[%s]\n", got, preedit());
        }
    }

    reset();
    tap(CHEWING_PLUS_KEY_SPACE, ' ',
        CHEWING_PLUS_MOD_CTRL | CHEWING_PLUS_MOD_SHIFT);
    check(chewing_plus_panel(session) == CHEWING_PLUS_PANEL_ENGLISH,
          "Ctrl+Shift+Space opens English");
    type_text("iphone");
    tap(CHEWING_PLUS_KEY_NONE, '2', CHEWING_PLUS_MOD_CTRL);
    tap(CHEWING_PLUS_KEY_RETURN, 0, 0);
    {
        const char *got = chewing_plus_take_commit(session);
        check(strcmp(got, "iphone") == 0, "Ctrl+2 picks the typed casing");
        if (strcmp(got, "iphone") != 0) {
            fprintf(stderr, "  commit=[%s]\n", got);
            dump_cands();
        }
    }

    reset();
    type_text("su3");
    type_text("github");
    check(strstr(aux(), "GitHub") != NULL, "English hint");
    check(strstr(aux(), "Control+Shift+Space") != NULL,
          "hint names the shortcut");
    if (!strstr(aux(), "GitHub")) {
        dump_cands();
    }
    reset();
    type_text("vu0");
    check(aux()[0] == '\0', "no hint for real bopomofo");
}

static void test_double_shift(void) {
    reset();
    tap(CHEWING_PLUS_KEY_SHIFT_R, 0, 0);
    tap(CHEWING_PLUS_KEY_SHIFT_R, 0, 0);
    check(chewing_plus_panel(session) == CHEWING_PLUS_PANEL_ENGLISH,
          "Right Shift x2 enters English");
    check(strstr(preedit(), "[EN]") != NULL, "Right Shift x2 preedit");
    if (chewing_plus_panel(session) != CHEWING_PLUS_PANEL_ENGLISH) {
        dump_cands();
    }
}

static void test_hint_commit(void) {
    const char *got;
    reset();
    type_text("su3github");
    check(strstr(aux(), "GitHub") != NULL, "hint names GitHub");
    tap(CHEWING_PLUS_KEY_SHIFT_R, 0, 0);
    tap(CHEWING_PLUS_KEY_SHIFT_R, 0, 0);
    check(chewing_plus_panel(session) == CHEWING_PLUS_PANEL_ENGLISH,
          "hint commit off: Right Shift x2 opens [EN]");
    reset();
    chewing_plus_set_english_hint_commit(session, 1);
    type_text("su3github");
    tap(CHEWING_PLUS_KEY_SHIFT_R, 0, 0);
    tap(CHEWING_PLUS_KEY_SHIFT_R, 0, 0);
    check(chewing_plus_panel(session) == CHEWING_PLUS_PANEL_NONE,
          "hint commit on: no [EN] panel");
    check(strstr(preedit(), "GitHub") != NULL &&
              strstr(preedit(), "你") != NULL,
          "hint commit on: GitHub replaces the reading, 你 kept");
    tap(CHEWING_PLUS_KEY_RETURN, 0, 0);
    got = chewing_plus_take_commit(session);
    check(strcmp(got, "你 GitHub ") == 0, "hint commit on: commit text");
    if (strcmp(got, "你 GitHub ") != 0) {
        fprintf(stderr, "  commit=[%s] preedit=[%s]\n", got, preedit());
    }
    reset();
    type_text("su3");
    tap(CHEWING_PLUS_KEY_SHIFT_R, 0, 0);
    tap(CHEWING_PLUS_KEY_SHIFT_R, 0, 0);
    check(chewing_plus_panel(session) == CHEWING_PLUS_PANEL_ENGLISH,
          "hint commit on, no hint: Right Shift x2 still opens [EN]");
    chewing_plus_set_english_hint_commit(session, 0);
}

static void test_suggestions(void) {
    const char *got;
    int i, found;
    chewing_plus_set_suggestions(session, 1, PLUS_PHRASES_PATH);
    reset();
    /* 中華 (ㄓㄨㄥ ㄏㄨㄚˊ: 5j/ cj86) -> 中華民國 among the suggestions */
    type_text("5j/ cj86");
    found = 0;
    for (i = 0; i < chewing_plus_suggestion_count(session); i++) {
        if (strcmp(chewing_plus_suggestion(session, i), "中華民國") == 0) {
            found = 1;
            chewing_plus_select_suggestion(session, i);
        }
    }
    check(found, "中華 suggests 中華民國");
    got = chewing_plus_take_commit(session);
    check(strcmp(got, "中華民國") == 0, "picking 中華民國 commits it");
    if (strcmp(got, "中華民國") != 0) {
        fprintf(stderr, "  commit=[%s] preedit=[%s]\n", got, preedit());
    }

    reset();
    /* ㄍㄒㄈㄘ: e v z h, one initial per syllable */
    type_text("evzh");
    check(chewing_plus_suggestion_count(session) >= 1 &&
              strcmp(chewing_plus_suggestion(session, 0), "恭喜發財") == 0,
          "ㄍㄒㄈㄘ suggests 恭喜發財");
    check(strstr(preedit(), "ㄍㄒㄈㄘ") != NULL, "preedit shows ㄍㄒㄈㄘ");
    chewing_plus_select_suggestion(session, 0);
    got = chewing_plus_take_commit(session);
    check(strcmp(got, "恭喜發財") == 0, "picking 恭喜發財 commits it");
    check(preedit()[0] == '\0', "reading cleared after the pick");

    reset();
    type_text("evzh");
    reset();
    /* ㄇㄇ (a a): two initials, default minimum 2 -> 媽媽 listed */
    type_text("aa");
    found = 0;
    for (i = 0; i < chewing_plus_suggestion_count(session); i++) {
        found |= strcmp(chewing_plus_suggestion(session, i), "媽媽") == 0;
    }
    check(found, "ㄇㄇ suggests 媽媽 (default minimum 2)");
    check(strstr(preedit(), "ㄇㄇ") != NULL, "preedit shows ㄇㄇ");
    check(chewing_plus_set_setting(session, "initials_min", "3") == 1,
          "initials_min setting");
    reset();
    type_text("aa");
    check(chewing_plus_suggestion_count(session) == 0,
          "minimum 3: two initials are not expanded");
    chewing_plus_set_setting(session, "initials_min", "1");
    reset();
    type_text("a");
    check(chewing_plus_suggestion_count(session) == 0,
          "minimum is never below 2");
    chewing_plus_set_setting(session, "initials_min", "2");
    reset();
    type_text("su3");
    check(chewing_plus_suggestion_count(session) == 0,
          "one character is not completed");

    chewing_plus_set_sensitive(session, 1);
    reset();
    type_text("evzh");
    check(chewing_plus_suggestion_count(session) == 0,
          "no suggestions while sensitive");
    chewing_plus_set_sensitive(session, 0);

    chewing_plus_set_suggestions(session, 0, NULL);
    reset();
    type_text("evzh");
    check(chewing_plus_suggestion_count(session) == 0,
          "suggestions off: nothing listed");
}

static void test_view(void) {
    chewing_plus_view v, w;
    reset();
    check(chewing_plus_get_view(NULL, &v) == 0, "get_view NULL session");
    check(chewing_plus_get_view(session, &v) == 1 && v.preedit[0] == '\0',
          "empty view");
    type_text("s");
    chewing_plus_get_view(session, &w);
    check(w.revision != v.revision && strcmp(w.preedit, "ㄋ") == 0 &&
              w.highlight_length > 0,
          "typing changes the view and its revision");
    /* A key release changes nothing on screen: same revision. */
    {
        chewing_plus_key_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.ascii = 's';
        ev.mods = CHEWING_PLUS_MOD_RELEASE;
        ev.time_ms = -1;
        chewing_plus_key(session, &ev);
    }
    chewing_plus_get_view(session, &v);
    check(v.revision == w.revision, "nothing visible changed: same revision");
    type_text("u3");
    chewing_plus_get_view(session, &v);
    check(strcmp(v.preedit, "你") == 0 && v.revision != w.revision,
          "view after 你");
}

static void press(uint32_t key, uint32_t mods) {
    chewing_plus_key_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.key = key;
    ev.mods = mods;
    ev.time_ms = clock_ms;
    clock_ms += 20;
    chewing_plus_key(session, &ev);
}

static void test_ascii_mode(void) {
    const char *got;
    chewing_plus_view v;
    reset();
    check(chewing_plus_ascii_mode(session) == 0, "Chinese mode by default");
    /* Lone Left Shift does nothing until the option is on. */
    press(CHEWING_PLUS_KEY_SHIFT_L, 0);
    press(CHEWING_PLUS_KEY_SHIFT_L, CHEWING_PLUS_MOD_RELEASE);
    check(chewing_plus_ascii_mode(session) == 0, "shift toggle off by default");

    chewing_plus_set_shift_toggle(session, 1);
    type_text("su3");
    press(CHEWING_PLUS_KEY_SHIFT_L, 0);
    press(CHEWING_PLUS_KEY_SHIFT_L, CHEWING_PLUS_MOD_RELEASE);
    check(chewing_plus_ascii_mode(session) == 1, "lone Left Shift -> ASCII");
    got = chewing_plus_take_commit(session);
    check(strcmp(got, "你") == 0, "switching to ASCII commits 你");
    chewing_plus_get_view(session, &v);
    check(v.preedit[0] == '\0', "nothing left after the switch");
    {
        chewing_plus_key_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.ascii = 's';
        ev.time_ms = clock_ms;
        check(chewing_plus_key(session, &ev) == 0,
              "ASCII mode passes letters to the app");
    }
    /* Shift used as a modifier is not a toggle. */
    press(CHEWING_PLUS_KEY_SHIFT_L, 0);
    {
        chewing_plus_key_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.ascii = 'A';
        ev.mods = CHEWING_PLUS_MOD_SHIFT;
        ev.time_ms = clock_ms;
        chewing_plus_key(session, &ev);
    }
    press(CHEWING_PLUS_KEY_SHIFT_L, CHEWING_PLUS_MOD_RELEASE);
    check(chewing_plus_ascii_mode(session) == 1, "Shift+A does not toggle");
    /* A long hold is not a tap. */
    press(CHEWING_PLUS_KEY_SHIFT_L, 0);
    clock_ms += 900;
    press(CHEWING_PLUS_KEY_SHIFT_L, CHEWING_PLUS_MOD_RELEASE);
    check(chewing_plus_ascii_mode(session) == 1, "a long hold does not toggle");
    press(CHEWING_PLUS_KEY_SHIFT_L, 0);
    press(CHEWING_PLUS_KEY_SHIFT_L, CHEWING_PLUS_MOD_RELEASE);
    check(chewing_plus_ascii_mode(session) == 0, "tap again -> Chinese");
    chewing_plus_set_ascii_mode(session, 1);
    check(chewing_plus_ascii_mode(session) == 1, "set_ascii_mode on");
    chewing_plus_set_shift_toggle(session, 0);
    check(chewing_plus_ascii_mode(session) == 0,
          "turning the option off returns to Chinese");
}

static void test_flush_and_page(void) {
    const char *got;
    chewing_plus_view v;
    reset();
    type_text("su3cl3");
    chewing_plus_flush(session, CHEWING_PLUS_FLUSH_DEFAULT);
    got = chewing_plus_take_commit(session);
    check(strcmp(got, "你好") == 0, "flush default commits 你好");
    reset();
    type_text("su3s");
    chewing_plus_flush(session, CHEWING_PLUS_FLUSH_PREEDIT);
    got = chewing_plus_take_commit(session);
    check(strcmp(got, "你ㄋ") == 0, "flush preedit commits the text as shown");
    reset();
    type_text("su3cl3");
    chewing_plus_flush(session, CHEWING_PLUS_FLUSH_CLEAR);
    got = chewing_plus_take_commit(session);
    chewing_plus_get_view(session, &v);
    check(got[0] == '\0' && v.preedit[0] == '\0', "flush clear drops it");

    reset();
    type_text("su3");
    tap(CHEWING_PLUS_KEY_DOWN, 0, 0);
    chewing_plus_get_view(session, &v);
    if (v.candidate_count > 0) {
        const char *first = chewing_plus_candidate(session, 0);
        char before[64];
        snprintf(before, sizeof(before), "%s", first ? first : "");
        check(chewing_plus_page(session, 1) == 1, "page forward");
        first = chewing_plus_candidate(session, 0);
        check(first && strcmp(first, before) != 0, "next page differs");
        chewing_plus_page(session, 0);
        first = chewing_plus_candidate(session, 0);
        check(first && strcmp(first, before) == 0, "page back to the first");
    } else {
        check(0, "candidates for 你");
    }

    /* Ctrl+Shift+Space can be turned off for frontends with their own key. */
    reset();
    chewing_plus_set_english_combo(session, 0);
    tap(CHEWING_PLUS_KEY_SPACE, ' ',
        CHEWING_PLUS_MOD_CTRL | CHEWING_PLUS_MOD_SHIFT);
    check(chewing_plus_panel(session) != CHEWING_PLUS_PANEL_ENGLISH,
          "combo off: Ctrl+Shift+Space does not open [EN]");
    chewing_plus_set_english_combo(session, 1);
    reset();
}

static void test_settings(void) {
    chewing_plus_setting_info info;
    int i, n = chewing_plus_setting_count();
    int seen_emoji = 0;
    const char *got;
    check(n >= 15, "settings table has entries");
    for (i = 0; i < n; i++) {
        check(chewing_plus_setting_info_at(i, &info) == 1 && info.key &&
                  info.key[0] && info.default_value && info.label_zh &&
                  info.label_zh[0],
              "setting row is complete");
        if (strcmp(info.key, "emoji") == 0) {
            seen_emoji = info.type == CHEWING_PLUS_SETTING_BOOL &&
                         strcmp(info.default_value, "0") == 0;
        }
    }
    check(seen_emoji, "emoji is a bool, off by default");
    check(chewing_plus_setting_info_at(n, &info) == 0, "row past the end");

    check(chewing_plus_set_setting(session, "no_such_key", "1") == 0,
          "unknown key ignored");
    check(chewing_plus_set_setting(session, "emoji", "maybe") == 0,
          "bad bool ignored");
    check(chewing_plus_set_setting(session, "page_size", "x") == 0,
          "bad int ignored");
    check(chewing_plus_set_setting(session, "calculator", "On") == 1,
          "bool accepts On");
    got = chewing_plus_get_setting(session, "calculator");
    check(got && strcmp(got, "1") == 0, "stored as 1");
    reset();
    type_text("=");
    check(chewing_plus_panel(session) == CHEWING_PLUS_PANEL_CALCULATOR,
          "calculator on through the table");
    chewing_plus_set_setting(session, "calculator", "false");
    reset();
    type_text("=");
    check(chewing_plus_panel(session) != CHEWING_PLUS_PANEL_CALCULATOR,
          "calculator off through the table");
    check(chewing_plus_set_setting(session, "page_size", "5") == 1 &&
              strcmp(chewing_plus_get_setting(session, "page_size"), "5") == 0,
          "int setting");
    chewing_plus_set_setting(session, "page_size", "10");
    check(chewing_plus_get_setting(session, "nope") == NULL,
          "get unknown key is NULL");

    /* suggestions through the table + phrase path */
    chewing_plus_set_phrases_path(session, PLUS_PHRASES_PATH);
    chewing_plus_set_setting(session, "suggestions", "1");
    reset();
    type_text("evzh");
    check(chewing_plus_suggestion_count(session) >= 1,
          "suggestions on through the table");
    chewing_plus_set_setting(session, "suggestions", "0");
    chewing_plus_set_setting(session, "calculator", "1");
    reset();
}

static void test_fuzzy_and_live(void) {
    chewing_plus_view v;
    const char *got;
    int i, found;
    /* Toneless: ㄋㄧㄏㄠㄇㄚ then Space -> 你好嗎 */
    check(chewing_plus_set_setting(session, "fuzzy", "1") == 1, "fuzzy on");
    reset();
    type_text("sucla8");
    chewing_plus_get_view(session, &v);
    check(strstr(v.preedit, "你好") != NULL,
          "toneless ㄋㄧㄏㄠ converts at the next initial");
    tap(CHEWING_PLUS_KEY_SPACE, ' ', 0);
    tap(CHEWING_PLUS_KEY_RETURN, 0, 0);
    got = chewing_plus_take_commit(session);
    check(strcmp(got, "你好嗎") == 0, "toneless commit 你好嗎");
    if (strcmp(got, "你好嗎") != 0) {
        fprintf(stderr, "  commit=[%s] preedit=[%s]\n", got, preedit());
    }
    /* Tones still work in fuzzy mode. */
    reset();
    type_text("su3cl3");
    check(strcmp(preedit(), "你好") == 0, "fuzzy mode keeps toned typing");
    chewing_plus_set_setting(session, "fuzzy", "0");
    reset();
    type_text("sucl");
    check(strstr(preedit(), "你") == NULL, "fuzzy off: no conversion");

    /* Live characters: ㄋ lists 你; picking it replaces the reading. */
    chewing_plus_set_chars_path(session, PLUS_CHARS_PATH);
    chewing_plus_set_setting(session, "live_chars", "1");
    reset();
    type_text("s");
    found = 0;
    for (i = 0; i < chewing_plus_suggestion_count(session); i++) {
        found |= strcmp(chewing_plus_suggestion(session, i), "你") == 0;
    }
    check(found, "ㄋ lists 你");
    type_text("u");
    check(chewing_plus_suggestion_count(session) > 0 &&
              strcmp(chewing_plus_suggestion(session, 0), "你") == 0,
          "ㄋㄧ lists 你 first");
    reset();
    type_text("su3s");
    for (i = 0; i < chewing_plus_suggestion_count(session); i++) {
        if (strcmp(chewing_plus_suggestion(session, i), "你") == 0) {
            chewing_plus_select_suggestion(session, i);
            break;
        }
    }
    got = chewing_plus_take_commit(session);
    check(strcmp(got, "你你") == 0, "picking keeps the buffer: 你 + 你");
    /* Off: nothing listed for a lone ㄋ. */
    chewing_plus_set_setting(session, "live_chars", "0");
    reset();
    type_text("s");
    check(chewing_plus_suggestion_count(session) == 0, "live chars off");

    /* Everything on together: toneless typing must not eat initials or the
     * English hint (it converts each initial to a character as it goes). */
    chewing_plus_set_setting(session, "fuzzy", "1");
    chewing_plus_set_setting(session, "live_chars", "1");
    chewing_plus_set_setting(session, "suggestions", "1");
    reset();
    type_text("su3evzh");
    check(strcmp(preedit(), "你ㄍㄒㄈㄘ") == 0,
          "toneless + initials: preedit shows 你ㄍㄒㄈㄘ");
    chewing_plus_select_suggestion(session, 0);
    got = chewing_plus_take_commit(session);
    check(strcmp(got, "你恭喜發財") == 0, "toneless + initials: 你恭喜發財");
    if (strcmp(got, "你恭喜發財") != 0) {
        fprintf(stderr, "  commit=[%s]\n", got);
    }
    chewing_plus_set_english_hint_commit(session, 1);
    reset();
    type_text("su3github");
    chewing_plus_toggle_english(session);
    check(strcmp(preedit(), "你GitHub ") == 0,
          "toneless + English hint: 你GitHub");
    chewing_plus_set_english_hint_commit(session, 0);
    chewing_plus_set_setting(session, "suggestions", "0");
    chewing_plus_set_setting(session, "fuzzy", "0");
    chewing_plus_set_setting(session, "live_chars", "0");
    reset();
}

static void test_fuzzy_user_dict(const char *sys) {
    char tmpl[] = "/tmp/chewing-plus-fuzzy-ud-XXXXXX";
    char *dir = mkdtemp(tmpl);
    char v4[256];
    char csv[256];
    FILE *f;
    chewing_plus_session *saved = session;
    chewing_plus_session *ud_session;
    if (!dir) {
        check(0, "fuzzy user dict temp dir");
        return;
    }
    snprintf(v4, sizeof(v4), "%s/v4", dir);
    snprintf(csv, sizeof(csv), "%s/v4/user_dict.csv", dir);
    check(mkdir(v4, 0700) == 0, "fuzzy user dict v4 dir");
    f = fopen(csv, "w");
    if (f) {
        fputs("設備,ㄕㄜˋ ㄅㄟˋ,0\n"
              "設定,ㄕㄜˋ ㄉㄧㄥˋ,0\n"
              "設計,ㄕㄜˋ ㄐㄧˋ,0\n",
              f);
        fclose(f);
    }
    ud_session = chewing_plus_new(sys, dir);
    check(ud_session != NULL, "fuzzy user dict session");
    if (ud_session) {
        session = ud_session;
        check(strcmp(chewing_plus_get_setting(session, "fuzzy"), "1") == 0,
              "fuzzy user dict starts with fuzzy on");
        type_text("gk4");
        check(strcmp(preedit(), "社") == 0,
              "fuzzy user dict: ㄕㄜˋ stays single char, not 設計");
        if (strcmp(preedit(), "社") != 0) {
            fprintf(stderr, "  preedit=[%s]\n", preedit());
        }
        tap(CHEWING_PLUS_KEY_DOWN, 0, 0);
        check(cand_has("社") && cand_has("設"),
              "fuzzy user dict: ㄕㄜˋ candidates remain single chars");
        session = saved;
        chewing_plus_free(ud_session);
    }
}

static void test_emoji_candidate(void) {
    int found;
    reset();
    type_text("d9 vup ");
    tap(CHEWING_PLUS_KEY_DOWN, 0, 0);
    found = cand_has("開心😊");
    check(found, "開心😊 offered among candidates");
    if (!found) {
        dump_cands();
    }
}

static void test_sensitive(void) {
    reset();
    chewing_plus_set_sensitive(session, 1);
    check(chewing_plus_sensitive(session) == 1, "sensitive flag");
    type_text("=");
    check(strstr(preedit(), "計算器") == NULL,
          "no calculator in a password field");
    check(chewing_plus_panel(session) == CHEWING_PLUS_PANEL_NONE,
          "no Plus panel while sensitive");
    check(chewing_plus_toggle_english(session) == 0,
          "English toggle disabled while sensitive");
    chewing_plus_insert_text(session, "hello");
    check(chewing_plus_take_commit(session)[0] == '\0',
          "dictation insert disabled while sensitive");
    tap(CHEWING_PLUS_KEY_SHIFT_R, 0, 0);
    tap(CHEWING_PLUS_KEY_SHIFT_R, 0, 0);
    check(chewing_plus_panel(session) == CHEWING_PLUS_PANEL_NONE,
          "double-tap Shift disabled while sensitive");

    chewing_plus_set_sensitive(session, 0);
    reset();
    chewing_plus_insert_text(session, "你好A");
    {
        const char *got = chewing_plus_take_commit(session);
        check(strcmp(got, "你好 A") == 0, "spacing returns when not sensitive");
        if (strcmp(got, "你好 A") != 0) {
            fprintf(stderr, "  commit=[%s]\n", got);
        }
    }
}

int main(void) {
    const char *sys = find_sys_path();
    char user_tmpl[] = "/tmp/chewing-plus-abi-XXXXXX";
    char *user;
    chewing_plus_callbacks cb;
    size_t phrase_count = 42;
    char **phrases;

    check(chewing_plus_abi_version() == CHEWING_PLUS_ABI_VERSION,
          "ABI version");
    check(strcmp(chewing_plus_libchewing_pin(), "0.14.0-rc.1-17-gdad3241") == 0,
          "libchewing pin");
    check(chewing_plus_key(NULL, NULL) == 0, "NULL session is a no-op");

    if (!sys) {
        fprintf(stderr,
                "libchewing 0.14 data not found (set CHEWING_TEST_DATA to the "
                "directory containing static_dict.bin)\n");
        return 1;
    }
    user = mkdtemp(user_tmpl);
    if (!user) {
        perror("mkdtemp");
        return 1;
    }
    session = chewing_plus_new(sys, user);
    if (!session) {
        fprintf(stderr, "chewing_plus_new failed for %s\n", sys);
        return 1;
    }
    check(strcmp(chewing_plus_get_setting(session, "fuzzy"), "1") == 0 &&
              strcmp(chewing_plus_get_setting(session, "live_chars"), "1") == 0,
          "toneless typing and live characters are on by default");
    /* The tests below were written for toned typing; test_fuzzy_and_live
     * turns these back on. */
    chewing_plus_set_setting(session, "fuzzy", "0");
    chewing_plus_set_setting(session, "live_chars", "0");
    memset(&cb, 0, sizeof(cb));
    cb.commit = on_commit;
    chewing_plus_set_callbacks(session, &cb);
    chewing_plus_set_data_paths(session, PLUS_EMOJI_PATH, "", PLUS_ENGLISH_PATH,
                                "");
    chewing_plus_set_emoji(session, 1);
    chewing_plus_set_calculator(session, 1);
    chewing_plus_set_spacing(session, 1, 1);
    chewing_plus_set_emoji_shortcode(session, 1, ";;", 300);
    chewing_plus_set_english(session, 1, 1, "Control+Shift+Space");
    chewing_plus_set_english_double_tap(session, 1, 300);

    phrases = chewing_plus_user_phrases(session, &phrase_count);
    check(phrases == NULL && phrase_count == 0, "empty user phrase list");
    chewing_plus_free_strings(phrases);

    test_calculator();
    test_spacing();
    test_shortcode();
    test_english();
    test_double_shift();
    test_hint_commit();
    test_suggestions();
    test_view();
    test_ascii_mode();
    test_flush_and_page();
    test_settings();
    test_fuzzy_and_live();
    test_fuzzy_user_dict(sys);
    test_emoji_candidate();
    test_sensitive();

    chewing_plus_free(session);
    printf(failures ? "ABI TESTS FAILED (%d)\n" : "ABI TESTS OK\n", failures);
    return failures ? 1 : 0;
}
