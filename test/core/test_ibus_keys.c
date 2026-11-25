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
    check(d.cancel_voice && d.action == CHEWING_IBUS_KEY_TO_CORE &&
              d.event.ascii == 'a',
          "typing while recording cancels and still types the key");
    d = chewing_ibus_map_key(0xffe1, 50, 0, 0, 1);
    check(!d.cancel_voice, "a modifier while recording does not cancel");

    /* Translate key: Right Ctrl, only when a translate key is configured. */
    d = chewing_ibus_map_key_ex(0xffe4, 105, 0, 0, 0, 0xffea, 0);
    check(d.action != CHEWING_IBUS_KEY_TRANSLATE_START,
          "Right Ctrl is a plain modifier while translation is off");
    d = chewing_ibus_map_key_ex(0xffe4, 105, 0, 0, 0, 0xffea, 0xffe4);
    check(d.action == CHEWING_IBUS_KEY_TRANSLATE_START,
          "Right Ctrl starts translation when on");
    d = chewing_ibus_map_key_ex(0xffe4, 105, 0, 1, 0, 0xffea, 0xffe4);
    check(d.action == CHEWING_IBUS_KEY_IGNORED,
          "no translation in a password field");
    d = chewing_ibus_map_key_ex(0xffe4, 105, 1u << 30, 0, 0xffe4, 0xffea,
                                0xffe4);
    check(d.action == CHEWING_IBUS_KEY_VOICE_STOP,
          "Right Ctrl release stops translation");
    d = chewing_ibus_map_key_ex(0xffe4, 105, 1u << 30, 0, 0xffea, 0xffea,
                                0xffe4);
    check(d.action != CHEWING_IBUS_KEY_VOICE_STOP,
          "Right Ctrl release does not stop a dictation started by Right Alt");
    d = chewing_ibus_map_key_ex(0xffea, 108, 1u << 30, 0, 0xffe4, 0xffea,
                                0xffe4);
    check(d.action != CHEWING_IBUS_KEY_VOICE_STOP,
          "Right Alt release does not stop a translation");
    d = chewing_ibus_map_key_ex(0xffe2, 62, 0, 0, 0, 0xffea, 0xffe4);
    check(d.action == CHEWING_IBUS_KEY_TO_CORE &&
              d.event.key == CHEWING_PLUS_KEY_SHIFT_R,
          "Right Shift stays with the core (English double tap)");

    /* Configurable keys (ibus.conf voice_key / voice_translate_key). */
    check(chewing_ibus_parse_modifier("Alt_R") == 0xffea &&
              chewing_ibus_parse_modifier("Control_R") == 0xffe4 &&
              chewing_ibus_parse_modifier("Super_R") == 0xffec &&
              chewing_ibus_parse_modifier("Shift_R") == 0xffe2 &&
              chewing_ibus_parse_modifier("nope") == 0 &&
              chewing_ibus_parse_modifier("") == 0 &&
              chewing_ibus_parse_modifier(NULL) == 0,
          "parse X11 modifier names");
    check(chewing_ibus_modifier_usable(0xffea) &&
              !chewing_ibus_modifier_usable(0xffe2) &&
              !chewing_ibus_modifier_usable(0xffe1) &&
              !chewing_ibus_modifier_usable(0),
          "Shift is not a usable talk key");
    d = chewing_ibus_map_key_ex(0xffec, 134, 0, 0, 0, 0xffec, 0xffe9);
    check(d.action == CHEWING_IBUS_KEY_VOICE_START,
          "custom dictation key Super_R starts voice");
    d = chewing_ibus_map_key_ex(0xffea, 108, 0, 0, 0, 0xffec, 0xffe9);
    check(d.action != CHEWING_IBUS_KEY_VOICE_START,
          "default Right Alt is not dictation after a custom key");
    d = chewing_ibus_map_key_ex(0xffe9, 64, 0, 0, 0, 0xffec, 0xffe9);
    check(d.action == CHEWING_IBUS_KEY_TRANSLATE_START,
          "custom translate key Alt_L starts translation");
    d = chewing_ibus_map_key_ex(0xffe4, 105, 0, 0, 0, 0xffec, 0xffe9);
    check(d.action != CHEWING_IBUS_KEY_TRANSLATE_START,
          "default Right Ctrl is not translation after a custom key");
    d = chewing_ibus_map_key_ex(0xffea, 108, 0, 0, 0, 0xffea, 0xffea);
    check(d.action == CHEWING_IBUS_KEY_VOICE_START,
          "colliding keys: dictation wins");
    d = chewing_ibus_map_key_ex(0xffe2, 62, 0, 0, 0, 0xffea, 0xffe2);
    check(d.action == CHEWING_IBUS_KEY_TO_CORE &&
              d.event.key == CHEWING_PLUS_KEY_SHIFT_R,
          "Shift_R as translate key is ignored (English double tap)");
    d = chewing_ibus_map_key_ex(0xffe2, 62, 0, 0, 0, 0xffe2, 0xffe4);
    check(d.action != CHEWING_IBUS_KEY_VOICE_START &&
              d.event.key == CHEWING_PLUS_KEY_SHIFT_R,
          "Shift_R as dictation key is not itself a talk key");
    d = chewing_ibus_map_key_ex(0xffea, 108, 0, 0, 0, 0xffe2, 0xffe4);
    check(d.action == CHEWING_IBUS_KEY_VOICE_START,
          "unusable dictation key falls back to Right Alt");
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
    /* xkb shift:both_capslock_cancel: Shift release arrives as Caps_Lock. */
    d = chewing_ibus_map_key(0xffe5, 62, (1u << 30) | 1u, 0, 0);
    check(d.action == CHEWING_IBUS_KEY_TO_CORE &&
              d.event.key == CHEWING_PLUS_KEY_SHIFT_R &&
              (d.event.mods & CHEWING_PLUS_MOD_RELEASE),
          "Right Shift release as Caps_Lock (X11 62) is SHIFT_R");
    d = chewing_ibus_map_key(0xffe5, 54, (1u << 30) | 1u, 0, 0);
    check(d.action == CHEWING_IBUS_KEY_TO_CORE &&
              d.event.key == CHEWING_PLUS_KEY_SHIFT_R &&
              (d.event.mods & CHEWING_PLUS_MOD_RELEASE),
          "Right Shift release as Caps_Lock (evdev 54) is SHIFT_R");
    d = chewing_ibus_map_key(0xffe5, 50, (1u << 30) | 1u, 0, 0);
    check(d.action == CHEWING_IBUS_KEY_TO_CORE &&
              d.event.key == CHEWING_PLUS_KEY_SHIFT_L &&
              (d.event.mods & CHEWING_PLUS_MOD_RELEASE),
          "Left Shift release as Caps_Lock (X11 50) is SHIFT_L");
    d = chewing_ibus_map_key(0xffe5, 42, (1u << 30) | 1u, 0, 0);
    check(d.action == CHEWING_IBUS_KEY_TO_CORE &&
              d.event.key == CHEWING_PLUS_KEY_SHIFT_L &&
              (d.event.mods & CHEWING_PLUS_MOD_RELEASE),
          "Left Shift release as Caps_Lock (evdev 42) is SHIFT_L");
    d = chewing_ibus_map_key(0xffe5, 66, 1u << 30, 0, 0);
    check(d.action == CHEWING_IBUS_KEY_IGNORED,
          "Caps_Lock key (X11 66) is not treated as Shift");
    printf(failures ? "IBUS KEY TESTS FAILED (%d)\n" : "IBUS KEY TESTS OK\n",
           failures);
    return failures ? 1 : 0;
}
