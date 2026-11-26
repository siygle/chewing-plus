/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * IBus setup program. libadwaita is linked only here, not by the engine.
 * Models are managed in chewing-plus-model-manager (a button here opens it).
 * Plus toggles are the same file the engine reads on focus.
 */
#include "chewing_plus.h"
#include "prefs.h"

#include <adwaita.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int loading;
    /* One switch per boolean setting from the core's table. */
    GtkWidget *switches[CHEWING_PLUS_IBUS_MAX_PREFS];
    const char *switch_keys[CHEWING_PLUS_IBUS_MAX_PREFS];
    int switch_count;
    GtkWidget *initials_min;
    GtkWidget *voice_switch;
    GtkWidget *voice_note;
    GtkWidget *voice_key;
    GtkWidget *translate_key;
    GtkWidget *key_note;
} Setup;

static Setup *UI;

/* Same X11 names as fcitx5 conf/voxkit.conf Hotkey / TranslateHotkey.
 * Shift is omitted: Right Shift is the English-mode double tap. */
static const char *const kTalkKeys[][2] = {
    {"Alt_R", "右 Alt"},
    {"Alt_L", "左 Alt"},
    {"Control_R", "右 Ctrl"},
    {"Control_L", "左 Ctrl"},
    {"Super_R", "右 Super"},
    {"Super_L", "左 Super"},
    {NULL, NULL},
};

static const char *combo_id(GtkWidget *combo) {
    const char *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
    return id && id[0] ? id : NULL;
}

static void set_combo_id(GtkWidget *combo, const char *id,
                         const char *fallback) {
    if (!id || !id[0] ||
        !gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), id)) {
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), fallback);
    }
}

static void refresh_key_note(void) {
    const char *voice;
    const char *translate;
    if (!UI->key_note) {
        return;
    }
    voice = combo_id(UI->voice_key);
    translate = combo_id(UI->translate_key);
    if (voice && translate && strcmp(voice, translate) == 0) {
        gtk_label_set_text(
            GTK_LABEL(UI->key_note),
            "語音和語音翻譯用了同一組快捷鍵，翻譯會被停用。請換一組。");
    } else {
        gtk_label_set_text(GTK_LABEL(UI->key_note), "");
    }
}

/* 「語音模型…」: the model window is its own program (chewing-plus-model-
 * manager), the same one the fcitx5 configuration page and the launcher open.
 * CHEWING_PLUS_MODEL_MANAGER in the environment overrides the path. */
static void on_open_models(GtkButton *button, gpointer user) {
    const char *program = g_getenv("CHEWING_PLUS_MODEL_MANAGER");
    GError *err = NULL;
    gchar *argv[2];
    (void)user;
    if (!program || !program[0]) {
        program = CHEWING_PLUS_MODEL_MANAGER_PATH;
    }
    argv[0] = (gchar *)program;
    argv[1] = NULL;
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
                       &err)) {
        gtk_button_set_label(button,
                             "找不到語音模型視窗，請重新安裝新酷音 Plus");
        g_clear_error(&err);
    }
}

/* Keep values for keys this window does not show (e.g. hand-edited
 * strings) and rewrite the switches it does. */
static void save_switches(void) {
    chewing_plus_ibus_prefs prefs;
    int i;
    if (UI->loading) {
        return;
    }
    chewing_plus_ibus_prefs_load(&prefs);
    for (i = 0; i < UI->switch_count; i++) {
        chewing_plus_ibus_prefs_set(
            &prefs, UI->switch_keys[i],
            gtk_switch_get_active(GTK_SWITCH(UI->switches[i])) ? "1" : "0");
    }
    if (UI->initials_min) {
        char value[16];
        snprintf(value, sizeof(value), "%d",
                 gtk_spin_button_get_value_as_int(
                     GTK_SPIN_BUTTON(UI->initials_min)));
        chewing_plus_ibus_prefs_set(&prefs, "initials_min", value);
    }
    if (UI->voice_key && combo_id(UI->voice_key)) {
        chewing_plus_ibus_prefs_set(&prefs, "voice_key",
                                    combo_id(UI->voice_key));
    }
    if (UI->translate_key && combo_id(UI->translate_key)) {
        chewing_plus_ibus_prefs_set(&prefs, "voice_translate_key",
                                    combo_id(UI->translate_key));
    }
    chewing_plus_ibus_prefs_save(&prefs);
}

static void on_spin(GtkSpinButton *spin, gpointer user) {
    (void)spin;
    (void)user;
    save_switches();
}

static void on_switch(GObject *object, GParamSpec *spec, gpointer user) {
    (void)object;
    (void)spec;
    (void)user;
    save_switches();
}

static void on_key_changed(GtkComboBox *combo, gpointer user) {
    (void)combo;
    (void)user;
    if (UI->loading) {
        return;
    }
    save_switches();
    refresh_key_note();
}

static GtkWidget *switch_row(const char *label, GtkWidget **out) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *text = gtk_label_new(label);
    GtkWidget *sw = gtk_switch_new();
    gtk_widget_set_hexpand(text, TRUE);
    gtk_label_set_xalign(GTK_LABEL(text), 0);
    gtk_box_append(GTK_BOX(box), text);
    gtk_box_append(GTK_BOX(box), sw);
    g_signal_connect(sw, "notify::active", G_CALLBACK(on_switch), NULL);
    *out = sw;
    return box;
}

static GtkWidget *button(const char *label, GCallback callback) {
    GtkWidget *widget = gtk_button_new_with_label(label);
    g_signal_connect(widget, "clicked", callback, NULL);
    return widget;
}

static void load_switches(void) {
    chewing_plus_ibus_prefs prefs;
    int i;
    chewing_plus_ibus_prefs_load(&prefs);
    UI->loading = 1;
    for (i = 0; i < UI->switch_count; i++) {
        const char *value =
            chewing_plus_ibus_prefs_get(&prefs, UI->switch_keys[i]);
        chewing_plus_setting_info info;
        int j;
        if (!value) {
            /* Not in the file yet: show the core's default. */
            for (j = 0; chewing_plus_setting_info_at(j, &info); j++) {
                if (strcmp(info.key, UI->switch_keys[i]) == 0) {
                    value = info.default_value;
                    break;
                }
            }
        }
        gtk_switch_set_active(GTK_SWITCH(UI->switches[i]),
                              value && strcmp(value, "1") == 0);
    }
    if (UI->initials_min) {
        const char *value = chewing_plus_ibus_prefs_get(&prefs, "initials_min");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(UI->initials_min),
                                  value ? atoi(value) : 2);
    }
    if (UI->voice_key) {
        set_combo_id(UI->voice_key,
                     chewing_plus_ibus_prefs_get(&prefs, "voice_key"), "Alt_R");
    }
    if (UI->translate_key) {
        set_combo_id(UI->translate_key,
                     chewing_plus_ibus_prefs_get(&prefs, "voice_translate_key"),
                     "Control_R");
    }
    refresh_key_note();
    UI->loading = 0;
}

/* 「啟用語音」: the voxkit user unit for this user only. pacman cannot ask
 * during install, so the switch lives here. */
static int run_systemctl(const char *const *args, int quiet) {
    GError *err = NULL;
    gint status = 0;
    gboolean ok = g_spawn_sync(
        NULL, (gchar **)args, NULL,
        G_SPAWN_SEARCH_PATH |
            (quiet ? (G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL)
                   : 0),
        NULL, NULL, NULL, NULL, &status, &err);
    if (!ok) {
        g_clear_error(&err);
        return -1;
    }
    return g_spawn_check_wait_status(status, NULL) ? 0 : 1;
}

static int voice_enabled(void) {
    const char *args[] = {"systemctl", "--user",         "is-enabled",
                          "--quiet",   "voxkit.service", NULL};
    return run_systemctl(args, 1) == 0;
}

static void refresh_voice_note(void) {
    const char *args[] = {"systemctl", "--user",         "is-active",
                          "--quiet",   "voxkit.service", NULL};
    gtk_label_set_text(GTK_LABEL(UI->voice_note),
                       run_systemctl(args, 1) == 0
                           ? "語音服務執行中（按住右 Alt 說話）。"
                           : "語音服務未執行。模型載入後約佔 260 MB 記憶體。");
}

static void on_voice_switch(GObject *object, GParamSpec *spec, gpointer user) {
    int on = gtk_switch_get_active(GTK_SWITCH(object));
    const char *enable[] = {"systemctl", "--user",         "enable",
                            "--now",     "voxkit.service", NULL};
    const char *disable[] = {"systemctl", "--user",         "disable",
                             "--now",     "voxkit.service", NULL};
    (void)spec;
    (void)user;
    if (UI->loading) {
        return;
    }
    if (run_systemctl(on ? enable : disable, 0) != 0) {
        gtk_label_set_text(GTK_LABEL(UI->voice_note),
                           on ? "無法啟用語音服務（systemctl --user 失敗）。"
                              : "無法停用語音服務。");
        UI->loading = 1;
        gtk_switch_set_active(GTK_SWITCH(object), voice_enabled());
        UI->loading = 0;
        return;
    }
    refresh_voice_note();
}

static void activate(GtkApplication *app, gpointer user) {
    GtkWidget *window;
    GtkWidget *root;
    GtkWidget *scroll;
    GtkWidget *page;
    GtkWidget *plus;
    (void)user;
    window = adw_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "新酷音 Plus");
    gtk_window_set_default_size(GTK_WINDOW(window), 520, 640);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(root), adw_header_bar_new());
    scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(page, 12);
    gtk_widget_set_margin_bottom(page, 12);
    gtk_widget_set_margin_start(page, 12);
    gtk_widget_set_margin_end(page, 12);

    {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        GtkWidget *text = gtk_label_new("啟用語音");
        gtk_widget_set_hexpand(text, TRUE);
        gtk_label_set_xalign(GTK_LABEL(text), 0);
        UI->voice_switch = gtk_switch_new();
        UI->loading = 1;
        gtk_switch_set_active(GTK_SWITCH(UI->voice_switch), voice_enabled());
        UI->loading = 0;
        g_signal_connect(UI->voice_switch, "notify::active",
                         G_CALLBACK(on_voice_switch), NULL);
        gtk_box_append(GTK_BOX(row), text);
        gtk_box_append(GTK_BOX(row), UI->voice_switch);
        gtk_box_append(GTK_BOX(page), row);
        UI->voice_note = gtk_label_new("");
        gtk_label_set_xalign(GTK_LABEL(UI->voice_note), 0);
        gtk_label_set_wrap(GTK_LABEL(UI->voice_note), TRUE);
        gtk_box_append(GTK_BOX(page), UI->voice_note);
        refresh_voice_note();
        {
            int i;
            GtkWidget *row;
            GtkWidget *text;
            UI->voice_key = gtk_combo_box_text_new();
            UI->translate_key = gtk_combo_box_text_new();
            for (i = 0; kTalkKeys[i][0]; i++) {
                gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(UI->voice_key),
                                          kTalkKeys[i][0], kTalkKeys[i][1]);
                gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(UI->translate_key),
                                          kTalkKeys[i][0], kTalkKeys[i][1]);
            }
            g_signal_connect(UI->voice_key, "changed",
                             G_CALLBACK(on_key_changed), NULL);
            g_signal_connect(UI->translate_key, "changed",
                             G_CALLBACK(on_key_changed), NULL);
            row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            text = gtk_label_new("語音快捷鍵（按住說話）");
            gtk_widget_set_hexpand(text, TRUE);
            gtk_label_set_xalign(GTK_LABEL(text), 0);
            gtk_box_append(GTK_BOX(row), text);
            gtk_box_append(GTK_BOX(row), UI->voice_key);
            gtk_box_append(GTK_BOX(page), row);
            row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            text =
                gtk_label_new("語音翻譯快捷鍵（按住說話；需打開「語音翻譯」）");
            gtk_widget_set_hexpand(text, TRUE);
            gtk_label_set_xalign(GTK_LABEL(text), 0);
            gtk_box_append(GTK_BOX(row), text);
            gtk_box_append(GTK_BOX(row), UI->translate_key);
            gtk_box_append(GTK_BOX(page), row);
            UI->key_note = gtk_label_new("");
            gtk_label_set_xalign(GTK_LABEL(UI->key_note), 0);
            gtk_label_set_wrap(GTK_LABEL(UI->key_note), TRUE);
            gtk_box_append(GTK_BOX(page), UI->key_note);
        }
    }
    {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        GtkWidget *text =
            gtk_label_new("語音模型（下載、刪除，語音辨識和翻譯分開管理）");
        gtk_widget_set_hexpand(text, TRUE);
        gtk_label_set_xalign(GTK_LABEL(text), 0);
        gtk_label_set_wrap(GTK_LABEL(text), TRUE);
        gtk_box_append(GTK_BOX(row), text);
        gtk_box_append(GTK_BOX(row),
                       button("語音模型…", G_CALLBACK(on_open_models)));
        gtk_box_append(GTK_BOX(page), row);
    }

    plus = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(plus),
                   gtk_label_new("Plus（下次聚焦輸入框時生效）"));
    {
        /* Every boolean setting in the core's table gets a switch, so new
         * settings show up here without touching this file. Layout and
         * selection-key options are left to the file. */
        static const char *const skip[] = {
            "space_as_selection", "add_phrase_forward", "choice_backward",
            "auto_shift_cursor", NULL};
        chewing_plus_setting_info info;
        int i;
        for (i = 0; chewing_plus_setting_info_at(i, &info) &&
                    UI->switch_count < CHEWING_PLUS_IBUS_MAX_PREFS;
             i++) {
            int k;
            int hidden = 0;
            if (info.type != CHEWING_PLUS_SETTING_BOOL) {
                continue;
            }
            for (k = 0; skip[k]; k++) {
                hidden |= strcmp(skip[k], info.key) == 0;
            }
            if (hidden) {
                continue;
            }
            UI->switch_keys[UI->switch_count] = info.key;
            gtk_box_append(
                GTK_BOX(plus),
                switch_row(info.label_zh, &UI->switches[UI->switch_count]));
            UI->switch_count++;
        }
    }
    {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        GtkWidget *text = gtk_label_new("字首注音幾個以上開始建議");
        gtk_widget_set_hexpand(text, TRUE);
        gtk_label_set_xalign(GTK_LABEL(text), 0);
        UI->initials_min = gtk_spin_button_new_with_range(2, 6, 1);
        g_signal_connect(UI->initials_min, "value-changed", G_CALLBACK(on_spin),
                         NULL);
        gtk_box_append(GTK_BOX(row), text);
        gtk_box_append(GTK_BOX(row), UI->initials_min);
        gtk_box_append(GTK_BOX(plus), row);
    }
    gtk_box_append(GTK_BOX(page), plus);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), page);
    gtk_box_append(GTK_BOX(root), scroll);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), root);
    load_switches();
    gtk_window_present(GTK_WINDOW(window));
}

static void shutdown_app(GApplication *app, gpointer user) {
    (void)app;
    (void)user;
}

int main(int argc, char **argv) {
    AdwApplication *app;
    int status;
    UI = g_new0(Setup, 1);
    app = adw_application_new("im.chewing.plus.ibus-setup", 0);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(shutdown_app), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    g_free(UI);
    return status;
}
