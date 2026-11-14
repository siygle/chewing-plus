/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * IBus keyvals use the same numbers as X keysyms. This mapper does not
 * include ibus.h so the headless test can link it without a running bus.
 */
#ifndef CHEWING_PLUS_IBUS_KEYS_H_
#define CHEWING_PLUS_IBUS_KEYS_H_

#include "chewing_plus.h"

#include <stdint.h>

/* Same bits as IBusModifierType. */
enum {
    CHEWING_IBUS_SHIFT = 1u << 0,
    CHEWING_IBUS_CONTROL = 1u << 2,
    CHEWING_IBUS_MOD1 = 1u << 3,
    CHEWING_IBUS_RELEASE = 1u << 30
};

enum {
    CHEWING_IBUS_KEY_IGNORED = 0,
    CHEWING_IBUS_KEY_TO_CORE,
    CHEWING_IBUS_KEY_VOICE_START,
    CHEWING_IBUS_KEY_VOICE_STOP,
    CHEWING_IBUS_KEY_VOICE_CANCEL
};

typedef struct chewing_ibus_decision {
    int action;
    chewing_plus_key_event event;
} chewing_ibus_decision;

/* Right Alt is push-to-talk, matching fcitx5-voxkit's default. A non-modifier
 * press while recording cancels the turn. `voice_active` is 1 after start
 * until stop or cancel. */
chewing_ibus_decision chewing_ibus_map_key(uint32_t keyval, uint32_t keycode,
                                           uint32_t state, int sensitive,
                                           int voice_active);

#endif
