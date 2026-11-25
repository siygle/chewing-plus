/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L

#include "prefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;

static void check(int ok, const char *what) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

static int is(const chewing_plus_ibus_prefs *p, const char *key,
              const char *want) {
    const char *got = chewing_plus_ibus_prefs_get(p, key);
    return want ? got && strcmp(got, want) == 0 : got == NULL;
}

int main(void) {
    char dir[] = "/tmp/ibus-prefs-XXXXXX";
    char path[256];
    FILE *old;
    chewing_plus_ibus_prefs saved;
    chewing_plus_ibus_prefs loaded;
    if (!mkdtemp(dir)) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(path, sizeof(path), "%s/ibus.conf", dir);
    setenv("CHEWING_PLUS_IBUS_CONFIG", path, 1);
    unsetenv("CHEWING_PLUS_IBUS_FEATURES");

    chewing_plus_ibus_prefs_load(&loaded);
    check(loaded.count == 0, "missing file loads empty");

    memset(&saved, 0, sizeof(saved));
    chewing_plus_ibus_prefs_set(&saved, "emoji", "1");
    chewing_plus_ibus_prefs_set(&saved, "shortcode_prefix", "``");
    chewing_plus_ibus_prefs_set(&saved, "emoji", "0");
    check(saved.count == 2 && is(&saved, "emoji", "0"), "set replaces");
    chewing_plus_ibus_prefs_set(&saved, "voice_key", "Super_R");
    chewing_plus_ibus_prefs_set(&saved, "voice_translate_key", "Alt_L");
    check(chewing_plus_ibus_prefs_save(&saved) == 0, "save");
    chewing_plus_ibus_prefs_load(&loaded);
    check(is(&loaded, "emoji", "0") && is(&loaded, "shortcode_prefix", "``") &&
              is(&loaded, "calculator", NULL),
          "file round trip, strings kept, absent stays absent");
    check(is(&loaded, "voice_key", "Super_R") &&
              is(&loaded, "voice_translate_key", "Alt_L"),
          "voice_key and voice_translate_key round trip like any other string");

    /* A file written by 5.1.13-plus.3 and earlier. */
    old = fopen(path, "w");
    fputs("emoji=1\nspace=1\nhint_commit=1\nshift_toggle=1\n", old);
    fclose(old);
    chewing_plus_ibus_prefs_load(&loaded);
    check(is(&loaded, "emoji", "1") && is(&loaded, "space_letters", "1") &&
              is(&loaded, "space_digits", "1") &&
              is(&loaded, "english_hint_commit", "1") &&
              is(&loaded, "shift_toggle", "1") && is(&loaded, "space", NULL),
          "old keys migrate");

    setenv("CHEWING_PLUS_IBUS_FEATURES", "calculator,suggest,hintcommit", 1);
    chewing_plus_ibus_prefs_load(&loaded);
    check(is(&loaded, "calculator", "1") && is(&loaded, "suggestions", "1") &&
              is(&loaded, "english_hint_commit", "1") &&
              is(&loaded, "emoji", NULL),
          "env overrides the file, old tokens work");
    unsetenv("CHEWING_PLUS_IBUS_FEATURES");
    chewing_plus_ibus_prefs_load(&loaded);
    check(is(&loaded, "emoji", "1"), "file returns when env is unset");

    if (failures) {
        fprintf(stderr, "%d failures\n", failures);
        return 1;
    }
    printf("IBUS PREFS TESTS OK\n");
    return 0;
}
