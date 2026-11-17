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

int main(void) {
    char dir[] = "/tmp/ibus-prefs-XXXXXX";
    char path[256];
    chewing_plus_ibus_prefs saved;
    chewing_plus_ibus_prefs loaded;
    if (!mkdtemp(dir)) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(path, sizeof(path), "%s/ibus.conf", dir);
    setenv("CHEWING_PLUS_IBUS_CONFIG", path, 1);
    unsetenv("CHEWING_PLUS_IBUS_FEATURES");

    memset(&saved, 0, sizeof(saved));
    saved.emoji = 1;
    saved.english = 1;
    saved.suggestions = 1;
    check(chewing_plus_ibus_prefs_save(&saved) == 0, "save");
    chewing_plus_ibus_prefs_load(&loaded);
    check(loaded.emoji && !loaded.calculator && !loaded.space &&
              !loaded.shortcode && loaded.english && loaded.suggestions &&
              !loaded.hint_commit && !loaded.easy_symbol,
          "file round trip");

    setenv("CHEWING_PLUS_IBUS_FEATURES", "calculator,shortcode,hintcommit", 1);
    chewing_plus_ibus_prefs_load(&loaded);
    check(!loaded.emoji && loaded.calculator && loaded.shortcode &&
              !loaded.english && loaded.hint_commit && !loaded.suggestions,
          "env overrides the file");
    unsetenv("CHEWING_PLUS_IBUS_FEATURES");
    chewing_plus_ibus_prefs_load(&loaded);
    check(loaded.emoji && loaded.english, "file returns when env is unset");

    if (failures) {
        fprintf(stderr, "%d failures\n", failures);
        return 1;
    }
    printf("IBUS PREFS TESTS OK\n");
    return 0;
}
