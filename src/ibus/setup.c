/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * IBus setup program. libadwaita is linked only here, not by the engine.
 * Model commands go through the shared voxkit C client. Plus toggles are
 * the same file the engine reads on focus.
 */
#include "chewing_plus.h"
#include "prefs.h"
#include "voxkit_client.h"

#include <adwaita.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    GtkWidget *status;
    GtkWidget *progress;
    GtkWidget *list;
    voxkit_client voice;
    guint timer;
    uint64_t req;
    int loading;
    char last_state[32];
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

static void format_bytes(char *buf, size_t n, uint64_t bytes) {
    if (bytes >= 1000000000ULL) {
        snprintf(buf, n, "%.1f GB", (double)bytes / 1e9);
    } else if (bytes >= 1000000ULL) {
        snprintf(buf, n, "%.1f MB", (double)bytes / 1e6);
    } else if (bytes >= 1000ULL) {
        snprintf(buf, n, "%.1f KB", (double)bytes / 1e3);
    } else {
        snprintf(buf, n, "%llu B", (unsigned long long)bytes);
    }
}

static int ensure(void) {
    if (voxkit_client_fd(&UI->voice) >= 0) {
        return 1;
    }
    return voxkit_client_connect(&UI->voice, NULL) == 0;
}

static const char *selected_id(void) {
    GtkListBoxRow *row;
    row = gtk_list_box_get_selected_row(GTK_LIST_BOX(UI->list));
    if (!row) {
        return NULL;
    }
    return g_object_get_data(G_OBJECT(row), "model-id");
}

static void set_status(const char *text) {
    gtk_label_set_text(GTK_LABEL(UI->status), text);
}

static void fail(const char *err, const char *fallback) {
    set_status(err && err[0] ? err : fallback);
}

static void show_message(const voxkit_message *msg) {
    char used[32];
    GString *text;
    const char *daemon = msg->daemon[0] ? msg->daemon : "running";
    const char *state = msg->state[0] ? msg->state : "idle";
    const char *active = msg->model[0] ? msg->model : "（未選）";
    format_bytes(used, sizeof(used), msg->store_bytes);
    text = g_string_new(NULL);
    g_string_printf(text, "服務 %s · %s · 已用 %s", daemon, state, used);
    g_string_append_printf(text, "\n目前 %s", active);
    if (msg->store[0]) {
        g_string_append_printf(text, "\n%s", msg->store);
    }
    if (msg->message[0]) {
        g_string_append_printf(text, "\n%s", msg->message);
    }
    set_status(text->str);
    g_string_free(text, TRUE);
    if (msg->bytes > 0) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(UI->progress),
                                      (double)msg->received /
                                          (double)msg->bytes);
    } else {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(UI->progress), 0);
    }
}

static void clear_list(void) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(UI->list)) != NULL) {
        gtk_list_box_remove(GTK_LIST_BOX(UI->list), child);
    }
}

static void add_row(const voxkit_model_info *info) {
    char size[32];
    const char *mark;
    GtkWidget *row;
    GtkWidget *box;
    GtkWidget *id;
    format_bytes(size, sizeof(size), info->bytes);
    if (info->active) {
        mark = "使用中";
    } else if (info->installed) {
        mark = "已安裝";
    } else {
        mark = "未安裝";
    }
    row = gtk_list_box_row_new();
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(box, 6);
    gtk_widget_set_margin_bottom(box, 6);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    id = gtk_label_new(info->id);
    gtk_widget_set_hexpand(id, TRUE);
    gtk_label_set_xalign(GTK_LABEL(id), 0);
    gtk_box_append(GTK_BOX(box), id);
    gtk_box_append(GTK_BOX(box), gtk_label_new(info->kind));
    gtk_box_append(GTK_BOX(box), gtk_label_new(mark));
    gtk_box_append(GTK_BOX(box), gtk_label_new(size));
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    g_object_set_data_full(G_OBJECT(row), "model-id", g_strdup(info->id),
                           g_free);
    gtk_list_box_append(GTK_LIST_BOX(UI->list), row);
}

static void refresh_list(void) {
    voxkit_model_info rows[VOXKIT_MODELS_MAX];
    voxkit_message summary;
    size_t count = 0;
    size_t i;
    char err[128] = "";
    if (!ensure()) {
        clear_list();
        set_status("voxkit 沒有在聽。");
        return;
    }
    memset(&summary, 0, sizeof(summary));
    if (voxkit_client_model_list(&UI->voice, ++UI->req, rows, VOXKIT_MODELS_MAX, &count,
                                 &summary, 1000, err, sizeof(err)) != 0) {
        fail(err, "讀不到模型列表。");
        return;
    }
    clear_list();
    for (i = 0; i < count; i++) {
        add_row(&rows[i]);
    }
    show_message(&summary);
}

static void on_refresh(GtkButton *button, gpointer user) {
    (void)button;
    (void)user;
    refresh_list();
}

static void on_install(GtkButton *button, gpointer user) {
    const char *id = selected_id();
    voxkit_message msg;
    char err[128] = "";
    (void)button;
    (void)user;
    if (!id || !ensure()) {
        return;
    }
    memset(&msg, 0, sizeof(msg));
    if (voxkit_client_model_install(&UI->voice, ++UI->req, id, &msg, 1000, err,
                                    sizeof(err)) != 0) {
        fail(err, "下載失敗。");
        return;
    }
    show_message(&msg);
}

static void on_cancel(GtkButton *button, gpointer user) {
    voxkit_message msg;
    (void)button;
    (void)user;
    if (!ensure()) {
        return;
    }
    memset(&msg, 0, sizeof(msg));
    if (voxkit_client_model_cancel(&UI->voice, ++UI->req, &msg, 500, NULL, 0) ==
        0) {
        show_message(&msg);
    }
    refresh_list();
}

static void on_remove(GtkButton *button, gpointer user) {
    const char *id = selected_id();
    voxkit_message msg;
    char err[128] = "";
    (void)button;
    (void)user;
    if (!id || !ensure()) {
        return;
    }
    memset(&msg, 0, sizeof(msg));
    if (voxkit_client_model_remove(&UI->voice, ++UI->req, id, &msg, 1000, err,
                                   sizeof(err)) != 0) {
        fail(err, "刪除失敗。");
        return;
    }
    refresh_list();
}

static void on_select(GtkButton *button, gpointer user) {
    const char *id = selected_id();
    voxkit_message msg;
    char err[128] = "";
    (void)button;
    (void)user;
    if (!id || !ensure()) {
        return;
    }
    memset(&msg, 0, sizeof(msg));
    if (voxkit_client_model_select(&UI->voice, ++UI->req, id, &msg, 1000, err,
                                   sizeof(err)) != 0) {
        fail(err, "選用失敗。");
        return;
    }
    refresh_list();
}

static gboolean poll_progress(gpointer user) {
    voxkit_message msg;
    (void)user;
    if (voxkit_client_fd(&UI->voice) < 0) {
        return G_SOURCE_CONTINUE;
    }
    memset(&msg, 0, sizeof(msg));
    if (voxkit_client_model_progress(&UI->voice, ++UI->req, &msg, 200, NULL,
                                     0) != 0) {
        return G_SOURCE_CONTINUE;
    }
    show_message(&msg);
    if (strcmp(msg.state, UI->last_state) != 0 &&
        (strcmp(msg.state, "done") == 0 || strcmp(msg.state, "error") == 0 ||
         strcmp(msg.state, "cancelled") == 0)) {
        snprintf(UI->last_state, sizeof(UI->last_state), "%s", msg.state);
        refresh_list();
        return G_SOURCE_CONTINUE;
    }
    snprintf(UI->last_state, sizeof(UI->last_state), "%s", msg.state);
    return G_SOURCE_CONTINUE;
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
    refresh_list();
}

static void activate(GtkApplication *app, gpointer user) {
    GtkWidget *window;
    GtkWidget *root;
    GtkWidget *scroll;
    GtkWidget *page;
    GtkWidget *buttons;
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
    gtk_box_append(GTK_BOX(page), gtk_label_new("語音模型"));
    UI->status = gtk_label_new("讀取中…");
    gtk_label_set_xalign(GTK_LABEL(UI->status), 0);
    gtk_label_set_wrap(GTK_LABEL(UI->status), TRUE);
    gtk_box_append(GTK_BOX(page), UI->status);
    UI->progress = gtk_progress_bar_new();
    gtk_box_append(GTK_BOX(page), UI->progress);
    UI->list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(UI->list),
                                    GTK_SELECTION_SINGLE);
    gtk_box_append(GTK_BOX(page), UI->list);

    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(buttons),
                   button("重新整理", G_CALLBACK(on_refresh)));
    gtk_box_append(GTK_BOX(buttons), button("下載", G_CALLBACK(on_install)));
    gtk_box_append(GTK_BOX(buttons), button("取消", G_CALLBACK(on_cancel)));
    gtk_box_append(GTK_BOX(buttons), button("刪除", G_CALLBACK(on_remove)));
    gtk_box_append(GTK_BOX(buttons), button("使用", G_CALLBACK(on_select)));
    gtk_box_append(GTK_BOX(page), buttons);

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
    refresh_list();
    UI->timer = g_timeout_add(400, poll_progress, NULL);
    gtk_window_present(GTK_WINDOW(window));
}

static void shutdown_app(GApplication *app, gpointer user) {
    (void)app;
    (void)user;
    if (UI->timer) {
        g_source_remove(UI->timer);
        UI->timer = 0;
    }
    voxkit_client_close(&UI->voice);
}

int main(int argc, char **argv) {
    AdwApplication *app;
    int status;
    UI = g_new0(Setup, 1);
    voxkit_client_init(&UI->voice);
    app = adw_application_new("im.chewing.plus.ibus-setup", 0);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(shutdown_app), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    g_free(UI);
    return status;
}
