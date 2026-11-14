/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * IBus Plus toggles. CHEWING_PLUS_IBUS_FEATURES overrides the file when it
 * is set. Otherwise the file is $CHEWING_PLUS_IBUS_CONFIG, or
 * $XDG_CONFIG_HOME/chewing-plus/ibus.conf, or
 * ~/.config/chewing-plus/ibus.conf.
 */
#ifndef CHEWING_PLUS_IBUS_PREFS_H_
#define CHEWING_PLUS_IBUS_PREFS_H_

typedef struct chewing_plus_ibus_prefs {
    int emoji;
    int calculator;
    int space;
    int shortcode;
    int english;
} chewing_plus_ibus_prefs;

void chewing_plus_ibus_prefs_load(chewing_plus_ibus_prefs *out);
/* 0 on success. Creates parent directories. */
int chewing_plus_ibus_prefs_save(const chewing_plus_ibus_prefs *in);

#endif
