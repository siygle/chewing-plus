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
} ChewingPlusEngine;

typedef struct {
    IBusEngineClass parent;
} ChewingPlusEngineClass;

static IBusEngineClass *parent_class = NULL;

static void on_commit(void *user, const char *utf8);

G_DEFINE_TYPE(ChewingPlusEngine, chewing_plus_engine, IBUS_TYPE_ENGINE)

static void refresh(ChewingPlusEngine *self);

static void apply_features(chewing_plus_session *session) {
    chewing_plus_ibus_prefs prefs;
    chewing_plus_ibus_prefs_load(&prefs);
    chewing_plus_set_emoji(session, prefs.emoji);
    chewing_plus_set_calculator(session, prefs.calculator);
    chewing_plus_set_spacing(session, prefs.space, prefs.space);
    chewing_plus_set_emoji_shortcode(session, prefs.shortcode, ";;", 300);
    chewing_plus_set_english(session, prefs.english, prefs.english,
                             "Control+Shift+Space");
    chewing_plus_set_english_double_tap(session, prefs.english, 300);
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
    {
        chewing_plus_callbacks cb;
        memset(&cb, 0, sizeof(cb));
        cb.commit = on_commit;
        cb.user = self;
        chewing_plus_set_callbacks(self->session, &cb);
    }
}

static void show_aux(IBusEngine *engine, const char *text) {
    IBusText *aux = ibus_text_new_from_string(text ? text : "");
    ibus_engine_update_auxiliary_text(engine, aux, text && text[0]);
    g_object_unref(aux);
}

static void refresh(ChewingPlusEngine *self) {
    IBusEngine *engine = IBUS_ENGINE(self);
    size_t cursor = 0;
    size_t hl_start = 0;
    size_t hl_len = 0;
    const char *preedit;
    const char *aux;
    int i;
    int n;
    IBusLookupTable *table;
    if (!self->session) {
        return;
    }
    preedit = chewing_plus_preedit(self->session, &cursor, &hl_start, &hl_len);
    aux = chewing_plus_aux(self->session);
    if (preedit && preedit[0]) {
        IBusText *text = ibus_text_new_from_string(preedit);
        ibus_text_append_attribute(text, IBUS_ATTR_TYPE_UNDERLINE,
                                   IBUS_ATTR_UNDERLINE_SINGLE, 0,
                                   (guint)strlen(preedit));
        if (hl_len) {
            ibus_text_append_attribute(text, IBUS_ATTR_TYPE_BACKGROUND,
                                       0x808080, (guint)hl_start,
                                       (guint)(hl_start + hl_len));
        }
        ibus_engine_update_preedit_text(engine, text, (guint)cursor, TRUE);
        g_object_unref(text);
    } else {
        IBusText *text = ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine, text, 0, FALSE);
        g_object_unref(text);
    }
    show_aux(engine, aux);
    n = chewing_plus_candidate_count(self->session);
    table = ibus_lookup_table_new(10, 0, TRUE, TRUE);
    for (i = 0; i < n; i++) {
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
    ibus_engine_update_lookup_table(engine, table, n > 0);
    g_object_unref(table);
}

static void on_commit(void *user, const char *utf8) {
    ChewingPlusEngine *self = user;
    IBusText *text = ibus_text_new_from_string(utf8 ? utf8 : "");
    ibus_engine_commit_text(IBUS_ENGINE(self), text);
    g_object_unref(text);
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
        chewing_plus_insert_text(self->session, msg->text);
        self->voice_active = 0;
        refresh(self);
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

static void voice_start(ChewingPlusEngine *self) {
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
            show_aux(IBUS_ENGINE(self), "語音服務未啟動（voxkit serve）");
            return;
        }
        voice_arm(self);
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
    if (voxkit_proto_write_start(line, sizeof(line), self->voice_req, "zh",
                                 convert, NULL, words, count) < 0 ||
        voxkit_client_send(&self->voice, line) != 0) {
        show_aux(IBUS_ENGINE(self), "語音服務未啟動（voxkit serve）");
        voice_close(self);
    } else {
        self->voice_active = 1;
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
    decision = chewing_ibus_map_key(keyval, keycode, state, sensitive(engine),
                                    self->voice_active);
    if (decision.action == CHEWING_IBUS_KEY_VOICE_START) {
        voice_start(self);
        return TRUE;
    }
    if (decision.action == CHEWING_IBUS_KEY_VOICE_STOP) {
        voice_command(self, "stop");
        return TRUE;
    }
    if (decision.action == CHEWING_IBUS_KEY_VOICE_CANCEL) {
        voice_command(self, "cancel");
        return TRUE;
    }
    if (decision.action != CHEWING_IBUS_KEY_TO_CORE) {
        return FALSE;
    }
    chewing_plus_key(self->session, &decision.event);
    refresh(self);
    return TRUE;
}

static void focus_in(IBusEngine *engine) {
    ChewingPlusEngine *self = (ChewingPlusEngine *)engine;
    parent_class->focus_in(engine);
    ensure_session(self);
    if (self->session) {
        apply_features(self->session);
    }
    apply_sensitive(self);
    self->voice_failed = 0;
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
}

static void dispose_engine(GObject *object) {
    ChewingPlusEngine *self = (ChewingPlusEngine *)object;
    voice_close(self);
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
}

static void chewing_plus_engine_init(ChewingPlusEngine *self) {
    init_engine(self);
}

void chewing_plus_ibus_register(IBusBus *bus) {
    IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(bus));
    ibus_factory_add_engine(factory, "chewing-plus",
                            chewing_plus_engine_get_type());
}
