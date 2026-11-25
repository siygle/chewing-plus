/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "keys.h"
#include "prefs.h"

#include "voxkit_client.h"

#include <glib-unix.h>
#include <ibus.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    IBusEngine parent;
    chewing_plus_session *session;
    voxkit_client voice;
    guint voice_watch;
    uint64_t voice_req;
    int voice_active;
    int voice_failed;
    uint32_t voice_key;    /* keyval that started the running recording */
    gint64 voice_since;    /* monotonic µs when that key went down */
    int voice_translating; /* the running request is mode=translate */
    int voice_caps;        /* translate capability: -1 not asked yet */
    uint32_t dictation_keyval; /* from ibus.conf voice_key; default Alt_R */
    uint32_t translate_keyval; /* from voice_translate_key; 0 if off */
    uint64_t drawn;        /* chewing_plus_view revision last drawn */
    IBusPropList *props;
    IBusProperty *mode_prop; /* 中 / 英 in the IBus panel */
    int shift_toggle;
} ChewingPlusEngine;

typedef struct {
    IBusEngineClass parent;
} ChewingPlusEngineClass;

static IBusEngineClass *parent_class = NULL;

static void on_commit(void *user, const char *utf8);
static int on_preceding_text(void *user, char *buf, size_t cap);

G_DEFINE_TYPE(ChewingPlusEngine, chewing_plus_engine, IBUS_TYPE_ENGINE)

static void refresh(ChewingPlusEngine *self);
static void refresh_talk_keys(ChewingPlusEngine *self);

static int last_shift_toggle = 0;

/* Apply every stored setting through the core's table. Absent keys keep the
 * core's defaults. */
static void apply_features(chewing_plus_session *session) {
    chewing_plus_ibus_prefs prefs;
    const char *shift;
    size_t i;
#ifndef CHEWING_PLUS_IBUS_PHRASES
#define CHEWING_PLUS_IBUS_PHRASES ""
#endif
    const char *phrases = getenv("CHEWING_PLUS_PHRASES");
#ifndef CHEWING_PLUS_IBUS_CHARS
#define CHEWING_PLUS_IBUS_CHARS ""
#endif
    const char *chars = getenv("CHEWING_PLUS_CHARS");
    chewing_plus_set_phrases_path(
        session, phrases && phrases[0] ? phrases : CHEWING_PLUS_IBUS_PHRASES);
    chewing_plus_set_chars_path(
        session, chars && chars[0] ? chars : CHEWING_PLUS_IBUS_CHARS);
    chewing_plus_ibus_prefs_load(&prefs);
    for (i = 0; i < prefs.count; i++) {
        chewing_plus_set_setting(session, prefs.items[i].key,
                                 prefs.items[i].value);
    }
    shift = chewing_plus_get_setting(session, "shift_toggle");
    last_shift_toggle = shift && strcmp(shift, "1") == 0;
    /* English mode: Ctrl+Shift+Space (on by default in the core) and Right
     * Shift tapped twice, as before 6.0.0. */
    {
        const char *english = chewing_plus_get_setting(session, "english");
        chewing_plus_set_english_double_tap(
            session, english && strcmp(english, "1") == 0, 300);
    }
}

static int sensitive(IBusEngine *engine) {
    guint purpose = IBUS_INPUT_PURPOSE_FREE_FORM;
    guint hints = 0;
    ibus_engine_get_content_type(engine, &purpose, &hints);
    return purpose == IBUS_INPUT_PURPOSE_PASSWORD ||
           purpose == IBUS_INPUT_PURPOSE_PIN;
}

static void apply_sensitive(ChewingPlusEngine *self) {
    if (self->session) {
        chewing_plus_set_sensitive(self->session, sensitive(IBUS_ENGINE(self)));
    }
}

static void ensure_session(ChewingPlusEngine *self) {
    const char *sys;
    const char *user;
    const char *emoji;
    const char *english;
    if (self->session) {
        return;
    }
    sys = getenv("CHEWING_PATH");
    user = getenv("CHEWING_USER_PATH");
    self->session = chewing_plus_new(sys, user);
    if (!self->session) {
        return;
    }
    emoji = getenv("CHEWING_PLUS_EMOJI");
    english = getenv("CHEWING_PLUS_ENGLISH");
#ifndef CHEWING_PLUS_IBUS_EMOJI
#define CHEWING_PLUS_IBUS_EMOJI ""
#endif
#ifndef CHEWING_PLUS_IBUS_ENGLISH
#define CHEWING_PLUS_IBUS_ENGLISH ""
#endif
    if (!emoji || !emoji[0]) {
        emoji = CHEWING_PLUS_IBUS_EMOJI;
    }
    if (!english || !english[0]) {
        english = CHEWING_PLUS_IBUS_ENGLISH;
    }
    chewing_plus_set_data_paths(self->session, emoji, "", english, "");
    apply_features(self->session);
    refresh_talk_keys(self);
    {
        chewing_plus_callbacks cb;
        memset(&cb, 0, sizeof(cb));
        cb.commit = on_commit;
        cb.user = self;
        chewing_plus_set_callbacks(self->session, &cb);
        chewing_plus_set_preceding_text(self->session, on_preceding_text, self);
    }
}

static void show_aux(IBusEngine *engine, const char *text) {
    IBusText *aux = ibus_text_new_from_string(text ? text : "");
    ibus_engine_update_auxiliary_text(engine, aux, text && text[0]);
    g_object_unref(aux);
}

/* 中 / 英 indicator. Shown only with the lone-Shift option. */
static void update_mode_prop(ChewingPlusEngine *self) {
    int english;
    IBusText *label;
    IBusText *symbol;
    if (!self->mode_prop || !self->session) {
        return;
    }
    english = chewing_plus_ascii_mode(self->session);
    label = ibus_text_new_from_string(english ? "英" : "中");
    symbol = ibus_text_new_from_string(english ? "英" : "中");
    ibus_property_set_label(self->mode_prop, label);
    ibus_property_set_symbol(self->mode_prop, symbol);
    ibus_property_set_visible(self->mode_prop, self->shift_toggle);
    ibus_engine_update_property(IBUS_ENGINE(self), self->mode_prop);
}

/* Phrase suggestions on the aux line under the preedit: "⇧1 中華民國 ⇧2 …".
 * The core's own aux text (English hint) comes first. */
static void show_suggestions(IBusEngine *engine, chewing_plus_session *session,
                             const char *aux, int count) {
    GString *line = g_string_new(aux ? aux : "");
    int i;
    for (i = 0; i < count && i < 9; i++) {
        const char *s = chewing_plus_suggestion(session, i);
        if (!s) {
            continue;
        }
        if (line->len) {
            g_string_append(line, "  ");
        }
        g_string_append_printf(line, "⇧%d %s", i + 1, s);
    }
    show_aux(engine, line->str);
    g_string_free(line, TRUE);
}

/* Draw the core's view. Skipped when nothing visible changed. */
static void refresh(ChewingPlusEngine *self) {
    IBusEngine *engine = IBUS_ENGINE(self);
    chewing_plus_view view;
    int i;
    IBusLookupTable *table;
    if (!self->session || !chewing_plus_get_view(self->session, &view)) {
        return;
    }
    if (view.revision == self->drawn) {
        return;
    }
    self->drawn = view.revision;
    if (view.preedit[0]) {
        IBusText *text = ibus_text_new_from_string(view.preedit);
        guint chars = (guint)g_utf8_strlen(view.preedit, -1);
        ibus_text_append_attribute(text, IBUS_ATTR_TYPE_UNDERLINE,
                                   IBUS_ATTR_UNDERLINE_SINGLE, 0, chars);
        if (view.highlight_length) {
            /* IBus attributes and cursor count characters, not bytes. */
            glong from = g_utf8_pointer_to_offset(
                view.preedit, view.preedit + view.highlight_start);
            glong to = g_utf8_pointer_to_offset(
                view.preedit,
                view.preedit + view.highlight_start + view.highlight_length);
            ibus_text_append_attribute(text, IBUS_ATTR_TYPE_BACKGROUND,
                                       0x808080, (guint)from, (guint)to);
        }
        ibus_engine_update_preedit_text(
            engine, text,
            (guint)g_utf8_pointer_to_offset(view.preedit,
                                            view.preedit + view.cursor),
            TRUE);
        g_object_unref(text);
    } else {
        IBusText *text = ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine, text, 0, FALSE);
        g_object_unref(text);
    }
    show_suggestions(engine, self->session, view.aux, view.suggestion_count);
    table = ibus_lookup_table_new(10, 0, TRUE, TRUE);
    for (i = 0; i < view.candidate_count; i++) {
        const char *label = chewing_plus_candidate_label(self->session, i);
        const char *cand = chewing_plus_candidate(self->session, i);
        IBusText *cand_text = ibus_text_new_from_string(cand ? cand : "");
        ibus_lookup_table_append_candidate(table, cand_text);
        g_object_unref(cand_text);
        if (label && label[0]) {
            IBusText *label_text = ibus_text_new_from_string(label);
            ibus_lookup_table_set_label(table, (guint)i, label_text);
            g_object_unref(label_text);
        }
    }
    ibus_engine_update_lookup_table(engine, table, view.candidate_count > 0);
    g_object_unref(table);
}

static void on_commit(void *user, const char *utf8) {
    ChewingPlusEngine *self = user;
    IBusText *text = ibus_text_new_from_string(utf8 ? utf8 : "");
    ibus_engine_commit_text(IBUS_ENGINE(self), text);
    g_object_unref(text);
}

/* Text before the cursor from the client's surrounding text (when the client
 * sends it), for spacing English quick insert after Chinese. */
static int on_preceding_text(void *user, char *buf, size_t cap) {
    ChewingPlusEngine *self = user;
    IBusText *text = NULL;
    guint cursor = 0;
    guint anchor = 0;
    const gchar *str;
    const gchar *end;
    const gchar *start;
    size_t len;

    if (!buf || cap < 2) {
        return 0;
    }
    ibus_engine_get_surrounding_text(IBUS_ENGINE(self), &text, &cursor,
                                     &anchor);
    str = text ? ibus_text_get_text(text) : NULL;
    if (!str || cursor == 0 || cursor > g_utf8_strlen(str, -1)) {
        return 0;
    }
    end = g_utf8_offset_to_pointer(str, cursor); /* cursor counts characters */
    start = end;
    while (start > str && (size_t)(end - start) + 4 < cap) {
        start = g_utf8_prev_char(start);
    }
    len = (size_t)(end - start);
    if (len >= cap) {
        return 0;
    }
    memcpy(buf, start, len);
    buf[len] = '\0';
    return (int)len;
}

static gboolean on_voice(gint fd, GIOCondition cond, gpointer user);

static void voice_stop_watch(ChewingPlusEngine *self) {
    if (self->voice_watch) {
        g_source_remove(self->voice_watch);
        self->voice_watch = 0;
    }
}

static void voice_close(ChewingPlusEngine *self) {
    voice_stop_watch(self);
    voxkit_client_close(&self->voice);
    self->voice_active = 0;
    self->voice_caps = -1; /* the next daemon may be a different version */
}

/* Shorter presses are accidental taps (or Right Alt/Ctrl used as a modifier
 * in a chord) and are cancelled instead of transcribed. */
#define VOICE_MIN_HOLD_US (250 * 1000)

static const char *translate_fallback_text(const char *reason) {
    if (!reason || !reason[0]) {
        return "翻譯失敗，已輸入原文";
    }
    if (strcmp(reason, "model-downloading") == 0) {
        return "翻譯模型下載中，已輸入原文";
    }
    if (strcmp(reason, "plugin-missing") == 0) {
        return "未安裝翻譯外掛（chewing-plus-translate），已輸入原文";
    }
    if (strcmp(reason, "bad-target") == 0) {
        return "不支援的翻譯語言，已輸入原文";
    }
    return "翻譯失敗，已輸入原文";
}

static void voice_arm(ChewingPlusEngine *self) {
    int fd = voxkit_client_fd(&self->voice);
    voice_stop_watch(self);
    if (fd < 0) {
        return;
    }
    self->voice_watch = g_unix_fd_add(fd, G_IO_IN | G_IO_HUP, on_voice, self);
}

static void voice_handle(ChewingPlusEngine *self, const voxkit_message *msg) {
    if (msg->req && msg->req != self->voice_req) {
        return;
    }
    if (msg->type == VOXKIT_MSG_FINAL && self->session) {
        /* A translation goes through the same insert path; the daemon never
         * applies the user dictionary to translated text. */
        chewing_plus_insert_text(self->session, msg->text);
        self->voice_active = 0;
        refresh(self);
        if (strcmp(msg->mode, "translate") == 0 && !msg->translated) {
            show_aux(IBUS_ENGINE(self),
                     translate_fallback_text(msg->translate_error));
        }
    } else if (msg->type == VOXKIT_MSG_PARTIAL) {
        show_aux(IBUS_ENGINE(self), msg->tentative);
    } else if (msg->type == VOXKIT_MSG_DISCARDED ||
               msg->type == VOXKIT_MSG_ERROR) {
        self->voice_active = 0;
        show_aux(IBUS_ENGINE(self),
                 msg->type == VOXKIT_MSG_ERROR ? msg->message : "");
    }
}

static gboolean on_voice(gint fd, GIOCondition cond, gpointer user) {
    ChewingPlusEngine *self = user;
    (void)fd;
    if (cond & (G_IO_HUP | G_IO_ERR)) {
        voice_close(self);
        show_aux(IBUS_ENGINE(self), "語音服務已斷線");
        return FALSE;
    }
    while (1) {
        voxkit_message msg;
        char err[64];
        int rc = voxkit_client_next(&self->voice, &msg, 0, err, sizeof(err));
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            voice_close(self);
            return FALSE;
        }
        voice_handle(self, &msg);
    }
    return TRUE;
}

static const char *translate_target(ChewingPlusEngine *self) {
    const char *target =
        self->session
            ? chewing_plus_get_setting(self->session, "voice_translate_to")
            : NULL;
    return target && target[0] ? target : "en";
}

static int translate_enabled(ChewingPlusEngine *self) {
    const char *on = self->session ? chewing_plus_get_setting(self->session,
                                                              "voice_translate")
                                   : NULL;
    return on && strcmp(on, "1") == 0;
}

/* voice_key / voice_translate_key in ibus.conf are X11 names, same as
 * fcitx5's Hotkey / TranslateHotkey. Applied on focus (下次聚焦輸入框時生效).
 */
static void refresh_talk_keys(ChewingPlusEngine *self) {
    chewing_plus_ibus_prefs prefs;
    const char *name;
    uint32_t key;
    chewing_plus_ibus_prefs_load(&prefs);
    name = chewing_plus_ibus_prefs_get(&prefs, "voice_key");
    key = chewing_ibus_parse_modifier(name);
    self->dictation_keyval =
        chewing_ibus_modifier_usable(key) ? key : CHEWING_IBUS_VOICE_KEY;
    self->translate_keyval = 0;
    if (!translate_enabled(self)) {
        return;
    }
    name = chewing_plus_ibus_prefs_get(&prefs, "voice_translate_key");
    key = chewing_ibus_parse_modifier(name);
    if (!key) {
        key = CHEWING_IBUS_TRANSLATE_KEY;
    }
    if (chewing_ibus_modifier_usable(key) && key != self->dictation_keyval) {
        self->translate_keyval = key;
    }
}

static void voice_start(ChewingPlusEngine *self, int translate) {
    char line[2048];
    const char *convert = "s2twp";
    char **phrases = NULL;
    size_t count = 0;
    const char **words = NULL;
    size_t i;
    if (self->voice_failed) {
        return;
    }
    if (voxkit_client_fd(&self->voice) < 0) {
        if (voxkit_client_connect(&self->voice, NULL) != 0) {
            self->voice_failed = 1;
            show_aux(IBUS_ENGINE(self),
                     "語音尚未啟用：請到新酷音 Plus 設定打開「啟用語音」");
            return;
        }
        voice_arm(self);
    }
    if (translate) {
        if (self->voice_caps < 0) {
            /* Once per connection; an older daemon has no capability and
             * must never get mode=translate. */
            self->voice_caps =
                voxkit_client_supports_translate(&self->voice, 400);
        }
        if (!self->voice_caps) {
            show_aux(IBUS_ENGINE(self), "語音服務版本太舊，不支援翻譯");
            return;
        }
    }
    self->voice_req++;
    if (self->session) {
        phrases = chewing_plus_user_phrases(self->session, &count);
    }
    if (count) {
        words = g_new0(const char *, count);
        for (i = 0; i < count; i++) {
            words[i] = phrases[i];
        }
    }
    if (voxkit_proto_write_start_mode(
            line, sizeof(line), self->voice_req, "zh", convert, NULL, words,
            count, translate ? "translate" : NULL,
            translate ? translate_target(self) : NULL) < 0 ||
        voxkit_client_send(&self->voice, line) != 0) {
        show_aux(IBUS_ENGINE(self),
                 "語音尚未啟用：請到新酷音 Plus 設定打開「啟用語音」");
        voice_close(self);
    } else {
        self->voice_active = 1;
        self->voice_translating = translate;
        self->voice_since = g_get_monotonic_time();
    }
    g_free(words);
    chewing_plus_free_strings(phrases);
}

static void voice_command(ChewingPlusEngine *self, const char *type) {
    char line[128];
    int n =
        snprintf(line, sizeof(line), "{\"v\":1,\"type\":\"%s\",\"req\":%llu}\n",
                 type, (unsigned long long)self->voice_req);
    if (n > 0 && (size_t)n < sizeof(line)) {
        voxkit_client_send(&self->voice, line);
    }
    if (strcmp(type, "cancel") == 0) {
        self->voice_active = 0;
    }
}

static gboolean process_key(IBusEngine *engine, guint keyval, guint keycode,
                            guint state) {
    ChewingPlusEngine *self = (ChewingPlusEngine *)engine;
    chewing_ibus_decision decision;
    ensure_session(self);
    if (!self->session) {
        return FALSE;
    }
    apply_sensitive(self);
    decision =
        chewing_ibus_map_key_ex(keyval, keycode, state, sensitive(engine),
                                self->voice_active ? self->voice_key : 0,
                                self->dictation_keyval, self->translate_keyval);
    if (decision.action == CHEWING_IBUS_KEY_VOICE_START ||
        decision.action == CHEWING_IBUS_KEY_TRANSLATE_START) {
        if (!self->voice_active) { /* else auto-repeat of the held key */
            self->voice_key = keyval;
            voice_start(self,
                        decision.action == CHEWING_IBUS_KEY_TRANSLATE_START);
        }
        return TRUE;
    }
    if (decision.action == CHEWING_IBUS_KEY_VOICE_STOP) {
        if (g_get_monotonic_time() - self->voice_since < VOICE_MIN_HOLD_US) {
            voice_command(self, "cancel");
            show_aux(engine, "");
        } else {
            voice_command(self, "stop");
        }
        return TRUE;
    }
    if (decision.cancel_voice) {
        /* Typing while recording cancels the turn, and the key still goes
         * where it was meant to (it used to be swallowed). */
        voice_command(self, "cancel");
        show_aux(engine, "");
    }
    if (decision.action != CHEWING_IBUS_KEY_TO_CORE) {
        return FALSE;
    }
    {
        /* Shift+1..9 (X11 keycodes 10..18) picks a phrase suggestion while
         * no candidate list or Plus panel is open. */
        chewing_plus_view view;
        int digit = (int)keycode - 9;
        if (!(state & CHEWING_IBUS_RELEASE) &&
            (state & (CHEWING_IBUS_SHIFT | CHEWING_IBUS_CONTROL |
                      CHEWING_IBUS_MOD1)) == CHEWING_IBUS_SHIFT &&
            digit >= 1 && digit <= 9 &&
            chewing_plus_get_view(self->session, &view) &&
            view.candidate_count == 0 &&
            view.panel == CHEWING_PLUS_PANEL_NONE &&
            digit <= view.suggestion_count) {
            chewing_plus_select_suggestion(self->session, digit - 1);
            refresh(self);
            return TRUE;
        }
    }
    {
        int was_english = chewing_plus_ascii_mode(self->session);
        int consumed = chewing_plus_key(self->session, &decision.event);
        if (was_english != chewing_plus_ascii_mode(self->session)) {
            update_mode_prop(self);
        }
        refresh(self);
        /* A key the core does not want (Backspace on an empty buffer, a
         * release, a lone modifier) goes back to the application. */
        return consumed ? TRUE : FALSE;
    }
}

static void focus_in(IBusEngine *engine) {
    ChewingPlusEngine *self = (ChewingPlusEngine *)engine;
    parent_class->focus_in(engine);
    ensure_session(self);
    if (self->session) {
        apply_features(self->session);
        refresh_talk_keys(self);
    }
    {
        /* Asks the client to send its surrounding text (spacing of English
         * quick insert); the answer arrives later. */
        IBusText *surrounding = NULL;
        guint cursor = 0;
        guint anchor = 0;
        ibus_engine_get_surrounding_text(engine, &surrounding, &cursor,
                                         &anchor);
    }
    self->shift_toggle = last_shift_toggle;
    apply_sensitive(self);
    self->voice_failed = 0;
    if (!self->props) {
        self->props = ibus_prop_list_new();
        g_object_ref_sink(self->props);
        self->mode_prop = ibus_property_new(
            "chewing-plus-mode", PROP_TYPE_NORMAL,
            ibus_text_new_from_string("中"), NULL,
            ibus_text_new_from_string("中／英（左 Shift 切換）"), TRUE, TRUE,
            PROP_STATE_UNCHECKED, NULL);
        ibus_prop_list_append(self->props, self->mode_prop);
    }
    ibus_engine_register_properties(engine, self->props);
    update_mode_prop(self);
    self->drawn = 0;
    refresh(self);
}

/* Clicking 中/英 in the IBus panel switches the mode too. */
static void property_activate(IBusEngine *engine, const gchar *name,
                              guint state) {
    ChewingPlusEngine *self = (ChewingPlusEngine *)engine;
    (void)state;
    if (!self->session || g_strcmp0(name, "chewing-plus-mode") != 0) {
        return;
    }
    chewing_plus_set_ascii_mode(self->session,
                                !chewing_plus_ascii_mode(self->session));
    update_mode_prop(self);
    refresh(self);
}

static void reset(IBusEngine *engine) {
    ChewingPlusEngine *self = (ChewingPlusEngine *)engine;
    if (self->voice_active) {
        voice_command(self, "cancel");
    }
    if (self->session) {
        chewing_plus_reset(self->session);
    }
    refresh(self);
}

static void disable(IBusEngine *engine) {
    reset(engine);
    parent_class->disable(engine);
}

static void candidate_clicked(IBusEngine *engine, guint index, guint button,
                              guint state) {
    ChewingPlusEngine *self = (ChewingPlusEngine *)engine;
    (void)button;
    (void)state;
    if (!self->session) {
        return;
    }
    chewing_plus_select(self->session, (int)index);
    refresh(self);
}

static void init_engine(ChewingPlusEngine *self) {
    voxkit_client_init(&self->voice);
    self->voice_caps = -1;
    self->dictation_keyval = CHEWING_IBUS_VOICE_KEY;
    self->translate_keyval = 0;
}

static void dispose_engine(GObject *object) {
    ChewingPlusEngine *self = (ChewingPlusEngine *)object;
    voice_close(self);
    if (self->props) {
        g_object_unref(self->props);
        self->props = NULL;
        self->mode_prop = NULL;
    }
    if (self->session) {
        chewing_plus_free(self->session);
        self->session = NULL;
    }
    G_OBJECT_CLASS(chewing_plus_engine_parent_class)->dispose(object);
}

static void chewing_plus_engine_class_init(ChewingPlusEngineClass *klass) {
    IBusEngineClass *engine = IBUS_ENGINE_CLASS(klass);
    GObjectClass *object = G_OBJECT_CLASS(klass);
    parent_class = g_type_class_peek_parent(klass);
    object->dispose = dispose_engine;
    engine->process_key_event = process_key;
    engine->focus_in = focus_in;
    engine->reset = reset;
    engine->disable = disable;
    engine->candidate_clicked = candidate_clicked;
    engine->property_activate = property_activate;
}

static void chewing_plus_engine_init(ChewingPlusEngine *self) {
    init_engine(self);
}

void chewing_plus_ibus_register(IBusBus *bus) {
    IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(bus));
    ibus_factory_add_engine(factory, "chewing-plus",
                            chewing_plus_engine_get_type());
}
