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
    K_CAPS_LOCK = 0xffe5,
    K_CONTROL_L = 0xffe3,
    K_CONTROL_R = 0xffe4,
    K_ALT_L = 0xffe9,
    K_ALT_R = 0xffea,
    K_META_L = 0xffe7,
    K_META_R = 0xffe8,
    K_SUPER_L = 0xffeb,
    K_SUPER_R = 0xffec
};

/* Physical Shift keys. IBus may report X11 keycodes (50/62, same as fcitx)
 * or evdev codes (42/54) depending on the seat. Under the xkb option
 * shift:both_capslock_cancel, the release keyval becomes Caps_Lock, so
 * Shift is also recognized by keycode. */
enum {
    X11_SHIFT_L = 50,
    X11_SHIFT_R = 62,
    EVDEV_SHIFT_L = 42,
    EVDEV_SHIFT_R = 54
};

static uint32_t shift_key(uint32_t keyval, uint32_t keycode) {
    if (keyval == K_SHIFT_R) {
        return CHEWING_PLUS_KEY_SHIFT_R;
    }
    if (keyval == K_SHIFT_L) {
        return CHEWING_PLUS_KEY_SHIFT_L;
    }
    if (keyval == K_CAPS_LOCK) {
        if (keycode == X11_SHIFT_R || keycode == EVDEV_SHIFT_R) {
            return CHEWING_PLUS_KEY_SHIFT_R;
        }
        if (keycode == X11_SHIFT_L || keycode == EVDEV_SHIFT_L) {
            return CHEWING_PLUS_KEY_SHIFT_L;
        }
    }
    return 0;
}

static int is_modifier(uint32_t keyval) {
    return keyval == K_SHIFT_L || keyval == K_SHIFT_R ||
           keyval == K_CONTROL_L || keyval == K_CONTROL_R ||
           keyval == K_ALT_L || keyval == K_ALT_R || keyval == K_META_L ||
           keyval == K_META_R || keyval == K_SUPER_L || keyval == K_SUPER_R;
}

/* Names match fcitx5's Key("Alt_R") / conf/voxkit.conf. */
static const struct {
    const char *name;
    uint32_t keyval;
} kModifiers[] = {{"Alt_R", K_ALT_R},
                  {"Alt_L", K_ALT_L},
                  {"Control_R", K_CONTROL_R},
                  {"Control_L", K_CONTROL_L},
                  {"Super_R", K_SUPER_R},
                  {"Super_L", K_SUPER_L},
                  {"Meta_R", K_META_R},
                  {"Meta_L", K_META_L},
                  {"Shift_R", K_SHIFT_R},
                  {"Shift_L", K_SHIFT_L},
                  {NULL, 0}};

uint32_t chewing_ibus_parse_modifier(const char *name) {
    size_t i;
    if (!name || !name[0]) {
        return 0;
    }
    for (i = 0; kModifiers[i].name; i++) {
        if (strcmp(name, kModifiers[i].name) == 0) {
            return kModifiers[i].keyval;
        }
    }
    return 0;
}

int chewing_ibus_modifier_usable(uint32_t keyval) {
    return keyval && keyval != K_SHIFT_L && keyval != K_SHIFT_R;
}

chewing_ibus_decision chewing_ibus_map_key(uint32_t keyval, uint32_t keycode,
                                           uint32_t state, int sensitive,
                                           int voice_active) {
    return chewing_ibus_map_key_ex(keyval, keycode, state, sensitive,
                                   voice_active ? CHEWING_IBUS_VOICE_KEY : 0,
                                   CHEWING_IBUS_VOICE_KEY, 0);
}

chewing_ibus_decision chewing_ibus_map_key_ex(uint32_t keyval, uint32_t keycode,
                                              uint32_t state, int sensitive,
                                              uint32_t voice_held,
                                              uint32_t dictation_key,
                                              uint32_t translate_key) {
    chewing_ibus_decision out;
    int release = (state & CHEWING_IBUS_RELEASE) != 0;
    uint32_t mods = 0;
    uint32_t shift;
    uint32_t dictate = chewing_ibus_modifier_usable(dictation_key)
                           ? dictation_key
                           : CHEWING_IBUS_VOICE_KEY;
    uint32_t translate = 0;
    int talk_key;
    if (chewing_ibus_modifier_usable(translate_key) &&
        translate_key != dictate) {
        translate = translate_key;
    }
    talk_key = keyval == dictate || (translate && keyval == translate);
    memset(&out, 0, sizeof(out));
    out.event.time_ms = -1;
    out.event.keycode = (int32_t)keycode;
    if (talk_key && (voice_held == 0 || voice_held == keyval)) {
        if (release) {
            out.action = voice_held ? CHEWING_IBUS_KEY_VOICE_STOP
                                    : CHEWING_IBUS_KEY_IGNORED;
        } else if (sensitive && !voice_held) {
            out.action = CHEWING_IBUS_KEY_IGNORED;
        } else {
            /* While recording this is auto-repeat; the engine ignores it. */
            out.action = keyval == dictate ? CHEWING_IBUS_KEY_VOICE_START
                                           : CHEWING_IBUS_KEY_TRANSLATE_START;
        }
        return out;
    }
    /* Typing while recording cancels the turn; the key itself still counts
     * (it used to be swallowed). */
    if (voice_held && !release && !is_modifier(keyval)) {
        out.cancel_voice = 1;
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
    shift = shift_key(keyval, keycode);
    if (shift) {
        out.event.key = shift;
        out.action = CHEWING_IBUS_KEY_TO_CORE;
        return out;
    }
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
