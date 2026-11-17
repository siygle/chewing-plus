/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * IBus settings store: the core's setting keys (chewing_plus_setting_info)
 * as "key=value" lines. The file is $CHEWING_PLUS_IBUS_CONFIG, or
 * $XDG_CONFIG_HOME/chewing-plus/ibus.conf, or ~/.config/chewing-plus/ibus.conf.
 * CHEWING_PLUS_IBUS_FEATURES ("emoji,english,...") turns those boolean
 * settings on instead of reading the file, for tests.
 *
 * Older files used "space" (both spacing settings) and "hint_commit"; they
 * are read as space_letters + space_digits and english_hint_commit.
 */
#ifndef CHEWING_PLUS_IBUS_PREFS_H_
#define CHEWING_PLUS_IBUS_PREFS_H_

#include <stddef.h>

#define CHEWING_PLUS_IBUS_MAX_PREFS 64

typedef struct chewing_plus_ibus_pref {
    char key[48];
    char value[64];
} chewing_plus_ibus_pref;

typedef struct chewing_plus_ibus_prefs {
    size_t count;
    chewing_plus_ibus_pref items[CHEWING_PLUS_IBUS_MAX_PREFS];
} chewing_plus_ibus_prefs;

/* Starts empty: a key that is absent means the setting's default. */
void chewing_plus_ibus_prefs_load(chewing_plus_ibus_prefs *out);
/* 0 on success. Creates parent directories. */
int chewing_plus_ibus_prefs_save(const chewing_plus_ibus_prefs *in);

/* NULL when not set. */
const char *chewing_plus_ibus_prefs_get(const chewing_plus_ibus_prefs *prefs,
                                        const char *key);
/* Adds or replaces. Returns 0, or -1 when full or too long. */
int chewing_plus_ibus_prefs_set(chewing_plus_ibus_prefs *prefs, const char *key,
                                const char *value);

#endif
