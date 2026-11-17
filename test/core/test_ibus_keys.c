/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "keys.h"

#include <stdio.h>

static int failures = 0;

static void check(int ok, const char *what) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

int main(void) {
    chewing_ibus_decision d;
    d = chewing_ibus_map_key('s', 31, 0, 0, 0);
    check(d.action == CHEWING_IBUS_KEY_TO_CORE && d.event.ascii == 's',
          "s is a core key");
    d = chewing_ibus_map_key(0xff0d, 28, 0, 0, 0);
    check(d.action == CHEWING_IBUS_KEY_TO_CORE &&
              d.event.key == CHEWING_PLUS_KEY_RETURN,
          "Return");
    d = chewing_ibus_map_key(0xffea, 108, 0, 0, 0);
    check(d.action == CHEWING_IBUS_KEY_VOICE_START, "Right Alt starts voice");
    d = chewing_ibus_map_key(0xffea, 108, 1u << 30, 0, 1);
    check(d.action == CHEWING_IBUS_KEY_VOICE_STOP, "Right Alt release stops");
    d = chewing_ibus_map_key(0xffea, 108, 0, 1, 0);
    check(d.action == CHEWING_IBUS_KEY_IGNORED, "no voice in a password field");
    d = chewing_ibus_map_key('a', 30, 0, 0, 1);
    check(d.action == CHEWING_IBUS_KEY_VOICE_CANCEL,
          "another key cancels the turn");
    d = chewing_ibus_map_key('A', 30, 1u << 0, 0, 0);
    check(d.event.mods & CHEWING_PLUS_MOD_SHIFT && d.event.ascii == 'A',
          "shift letter");
    /* Lone Left Shift: both the press and the release reach the core, so it
     * can tell a tap (中/英 switch) from Shift held for a capital. */
    d = chewing_ibus_map_key(0xffe1, 42, 0, 0, 0);
    check(d.action == CHEWING_IBUS_KEY_TO_CORE &&
              d.event.key == CHEWING_PLUS_KEY_SHIFT_L &&
              !(d.event.mods & CHEWING_PLUS_MOD_RELEASE),
          "Left Shift press reaches the core");
    d = chewing_ibus_map_key(0xffe1, 42, (1u << 30) | 1u, 0, 0);
    check(d.action == CHEWING_IBUS_KEY_TO_CORE &&
              d.event.key == CHEWING_PLUS_KEY_SHIFT_L &&
              (d.event.mods & CHEWING_PLUS_MOD_RELEASE),
          "Left Shift release reaches the core");
    printf(failures ? "IBUS KEY TESTS FAILED (%d)\n" : "IBUS KEY TESTS OK\n",
           failures);
    return failures ? 1 : 0;
}
