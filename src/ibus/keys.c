/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "keys.h"

#include <string.h>

enum {
    K_BACKSPACE = 0xff08,
    K_TAB = 0xff09,
    K_RETURN = 0xff0d,
    K_ESCAPE = 0xff1b,
    K_DELETE = 0xffff,
    K_HOME = 0xff50,
    K_LEFT = 0xff51,
    K_UP = 0xff52,
    K_RIGHT = 0xff53,
    K_DOWN = 0xff54,
    K_PAGE_UP = 0xff55,
    K_PAGE_DOWN = 0xff56,
    K_END = 0xff57,
    K_KP_ENTER = 0xff8d,
    K_SHIFT_L = 0xffe1,
    K_SHIFT_R = 0xffe2,
    K_CONTROL_L = 0xffe3,
    K_CONTROL_R = 0xffe4,
    K_ALT_L = 0xffe9,
    K_ALT_R = 0xffea
};

static int is_modifier(uint32_t keyval) {
    return keyval == K_SHIFT_L || keyval == K_SHIFT_R ||
           keyval == K_CONTROL_L || keyval == K_CONTROL_R ||
           keyval == K_ALT_L || keyval == K_ALT_R;
}

chewing_ibus_decision chewing_ibus_map_key(uint32_t keyval, uint32_t keycode,
                                           uint32_t state, int sensitive,
                                           int voice_active) {
    chewing_ibus_decision out;
    int release = (state & CHEWING_IBUS_RELEASE) != 0;
    uint32_t mods = 0;
    memset(&out, 0, sizeof(out));
    out.event.time_ms = -1;
    out.event.keycode = (int32_t)keycode;
    if (keyval == K_ALT_R) {
        if (release) {
            out.action = voice_active ? CHEWING_IBUS_KEY_VOICE_STOP
                                      : CHEWING_IBUS_KEY_IGNORED;
        } else if (!sensitive) {
            out.action = CHEWING_IBUS_KEY_VOICE_START;
        } else {
            out.action = CHEWING_IBUS_KEY_IGNORED;
        }
        return out;
    }
    if (voice_active && !release && !is_modifier(keyval)) {
        out.action = CHEWING_IBUS_KEY_VOICE_CANCEL;
        return out;
    }
    if (state & CHEWING_IBUS_SHIFT) {
        mods |= CHEWING_PLUS_MOD_SHIFT;
    }
    if (state & CHEWING_IBUS_CONTROL) {
        mods |= CHEWING_PLUS_MOD_CTRL;
    }
    if (state & CHEWING_IBUS_MOD1) {
        mods |= CHEWING_PLUS_MOD_ALT;
    }
    if (release) {
        mods |= CHEWING_PLUS_MOD_RELEASE;
    }
    out.event.mods = mods;
    if (keyval == K_BACKSPACE) {
        out.event.key = CHEWING_PLUS_KEY_BACKSPACE;
    } else if (keyval == K_TAB) {
        out.event.key = CHEWING_PLUS_KEY_TAB;
    } else if (keyval == K_RETURN || keyval == K_KP_ENTER) {
        out.event.key = CHEWING_PLUS_KEY_RETURN;
    } else if (keyval == K_ESCAPE) {
        out.event.key = CHEWING_PLUS_KEY_ESCAPE;
    } else if (keyval == K_DELETE) {
        out.event.key = CHEWING_PLUS_KEY_DELETE;
    } else if (keyval == K_HOME) {
        out.event.key = CHEWING_PLUS_KEY_HOME;
    } else if (keyval == K_LEFT) {
        out.event.key = CHEWING_PLUS_KEY_LEFT;
    } else if (keyval == K_UP) {
        out.event.key = CHEWING_PLUS_KEY_UP;
    } else if (keyval == K_RIGHT) {
        out.event.key = CHEWING_PLUS_KEY_RIGHT;
    } else if (keyval == K_DOWN) {
        out.event.key = CHEWING_PLUS_KEY_DOWN;
    } else if (keyval == K_PAGE_UP) {
        out.event.key = CHEWING_PLUS_KEY_PAGE_UP;
    } else if (keyval == K_PAGE_DOWN) {
        out.event.key = CHEWING_PLUS_KEY_PAGE_DOWN;
    } else if (keyval == K_END) {
        out.event.key = CHEWING_PLUS_KEY_END;
    } else if (keyval == K_SHIFT_R) {
        out.event.key = CHEWING_PLUS_KEY_SHIFT_R;
    } else if (keyval == K_SHIFT_L) {
        out.event.key = CHEWING_PLUS_KEY_SHIFT_L;
    } else if (keyval == ' ') {
        out.event.key = CHEWING_PLUS_KEY_SPACE;
        out.event.ascii = ' ';
    } else if (keyval > 0x20 && keyval < 0x7f) {
        out.event.ascii = keyval;
    } else if (keyval >= 0xffb0 && keyval <= 0xffb9) {
        out.event.ascii = (uint32_t)('0' + (keyval - 0xffb0));
    } else if (!release && is_modifier(keyval)) {
        out.action = CHEWING_IBUS_KEY_IGNORED;
        return out;
    } else {
        out.action = CHEWING_IBUS_KEY_IGNORED;
        return out;
    }
    out.action = CHEWING_IBUS_KEY_TO_CORE;
    return out;
}
