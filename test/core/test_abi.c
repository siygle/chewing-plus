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
    check(strcmp(chewing_plus_libchewing_pin(), "0.14.0-rc.1") == 0,
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
    test_emoji_candidate();
    test_sensitive();

    chewing_plus_free(session);
    printf(failures ? "ABI TESTS FAILED (%d)\n" : "ABI TESTS OK\n", failures);
    return failures ? 1 : 0;
}
