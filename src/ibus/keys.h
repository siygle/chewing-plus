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
    /* No longer returned: a key pressed while recording now sets
     * `cancel_voice` and is still handled (it used to be swallowed). */
    CHEWING_IBUS_KEY_VOICE_CANCEL,
    CHEWING_IBUS_KEY_TRANSLATE_START
};

/* Push-to-talk keys. Right Ctrl is the translate key; Right Shift is left
 * to the English-mode double tap. */
enum {
    CHEWING_IBUS_VOICE_KEY = 0xffea,    /* Alt_R */
    CHEWING_IBUS_TRANSLATE_KEY = 0xffe4 /* Control_R */
};

typedef struct chewing_ibus_decision {
    int action;
    chewing_plus_key_event event;
    /* A recording is running and this key cancels it. The engine cancels,
     * then treats the key as `action` says (core or application). */
    int cancel_voice;
} chewing_ibus_decision;

/* Right Alt is push-to-talk, matching fcitx5-voxkit's default. A non-modifier
 * press while recording cancels the turn. `voice_active` is 1 after start
 * until stop or cancel. Same as chewing_ibus_map_key_ex with the dictation
 * key and translation off. */
chewing_ibus_decision chewing_ibus_map_key(uint32_t keyval, uint32_t keycode,
                                           uint32_t state, int sensitive,
                                           int voice_active);

/* `voice_held` is the keyval that started the running recording (0 when
 * idle); only its release stops it. `dictation_key` is the configured
 * push-to-talk key (X11 keyval; 0 means Alt_R). `translate_key` is the
 * configured push-to-translate key; 0 turns translation off. If the two
 * are the same, dictation wins. Shift_L/Shift_R are never talk keys
 * (Right Shift is the English-mode double tap). */
chewing_ibus_decision chewing_ibus_map_key_ex(uint32_t keyval, uint32_t keycode,
                                              uint32_t state, int sensitive,
                                              uint32_t voice_held,
                                              uint32_t dictation_key,
                                              uint32_t translate_key);

/* X11 keysym name ("Alt_R", "Control_R", "Super_L", ...) to a keyval, or
 * 0 if it is not a lone modifier we accept. */
uint32_t chewing_ibus_parse_modifier(const char *name);
/* 0 for Shift_L/Shift_R (reserved for English) and for 0. */
int chewing_ibus_modifier_usable(uint32_t keyval);

#endif
