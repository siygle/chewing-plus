/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * 「語音模型」 window for Linux (GTK4 + libadwaita): the same window as the
 * macOS ModelManager.swift. Speech recognition (and voice detection) models
 * and translation models in two sections, one card per model with download,
 * resume, cancel, use, check and delete, and a license checkbox that gates
 * the download of restricted models (HY-MT). It uses the running voxkit
 * daemon, and `voxkit model ... --json` when the daemon is not running.
 *
 * Opened from the fcitx5 configuration page, the IBus setup window, or the
 * 「新酷音 Plus 語音模型」 launcher. libadwaita is linked only here.
 */
#include "backend.h"

#include <adwaita.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct App App;

typedef struct Card {
    App *app;
    char id[64];
    GtkWidget *status;
    GtkWidget *progress_box;
    GtkWidget *progress;
    GtkWidget *progress_text;
    GtkWidget *consent;
    GtkWidget *download;
    GtkWidget *cancel;
    GtkWidget *use;
    GtkWidget *verify;
    GtkWidget *remove;
} Card;

struct App {
    mm_backend backend;
    mm_snapshot snap;
    int have_snap;
    mm_transfer transfer;
    char last_state[32];
    GHashTable *consent;
    GPtrArray *cards;
    unsigned ticks;
    guint timer;

    GtkWidget *window;
    GtkWidget *usage;
    GtkWidget *dot;
    GtkWidget *service;
    GtkWidget *speech_subtitle;
    GtkWidget *speech_cards;
    GtkWidget *translate_subtitle;
    GtkWidget *translate_cards;
    GtkWidget *notice;
};

static App *UI;

static void rebuild(App *app);
static void refresh(App *app);
static void update_cards(App *app);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static const voxkit_model_info *find_row(const App *app, const char *id) {
    size_t i;
    for (i = 0; i < app->snap.count; i++) {
        if (strcmp(app->snap.rows[i].id, id) == 0) {
            return &app->snap.rows[i];
        }
    }
    return NULL;
}

static int transfer_busy(const mm_transfer *t) {
    return strcmp(t->state, "downloading") == 0 ||
           strcmp(t->state, "verifying") == 0;
}

static int someone_busy(const App *app) {
    return transfer_busy(&app->transfer) || mm_job_running(&app->backend);
}

static void show_notice(App *app, const char *text, int error) {
    gtk_label_set_text(GTK_LABEL(app->notice), text);
    gtk_widget_set_visible(app->notice, text[0] != '\0');
    if (error) {
        gtk_widget_remove_css_class(app->notice, "dim-label");
        gtk_widget_add_css_class(app->notice, "error");
    } else {
        gtk_widget_remove_css_class(app->notice, "error");
        gtk_widget_add_css_class(app->notice, "dim-label");
    }
}

static void show_notice_f(App *app, int error, const char *fmt, const char *a) {
    char text[1200];
    snprintf(text, sizeof(text), fmt, a);
    show_notice(app, text, error);
}

/* A refused request becomes a notice. 1 when it was a refusal. */
static int handle_status(App *app, const mm_status *status) {
    char text[1200];
    if (status->ok) {
        return 0;
    }
    mm_error_text(status->code, status->message, text, sizeof(text));
    show_notice(app, text, 1);
    return 1;
}

static const char *title_of(const App *app, const char *id) {
    const voxkit_model_info *row = find_row(app, id);
    return row ? row->title : id;
}

static void show_verify(App *app, const char *model, const char *result) {
    char text[1200];
    int bad = mm_verify_text(result, title_of(app, model), text, sizeof(text));
    show_notice(app, text, bad);
}

/* ------------------------------------------------------------------ */
/* Progress                                                            */
/* ------------------------------------------------------------------ */

static void apply_progress(App *app, const mm_transfer *next) {
    int changed = memcmp(&app->transfer, next, sizeof(*next)) != 0;
    if (changed) {
        app->transfer = *next;
        update_cards(app);
    }
    if (strcmp(next->state, app->last_state) != 0) {
        if (strcmp(next->state, "done") == 0 ||
            strcmp(next->state, "cancelled") == 0 ||
            strcmp(next->state, "error") == 0) {
            if (strcmp(next->state, "error") == 0) {
                show_notice(app,
                            next->message[0] ? next->message : "下載失敗。", 1);
            } else if (strcmp(next->state, "done") == 0 && next->message[0]) {
                show_verify(app, next->model, next->message);
            }
            snprintf(app->last_state, sizeof(app->last_state), "%s",
                     next->state);
            refresh(app);
            return;
        }
        snprintf(app->last_state, sizeof(app->last_state), "%s", next->state);
    }
}

static void pump_job(App *app) {
    mm_event ev;
    while (mm_job_pump(&app->backend, &ev) != MM_EVENT_NONE) {
        if (ev.type == MM_EVENT_PROGRESS) {
            if (strcmp(ev.transfer.state, "downloading") == 0 ||
                strcmp(ev.transfer.state, "verifying") == 0 ||
                strcmp(ev.transfer.state, "done") == 0) {
                apply_progress(app, &ev.transfer);
            }
        } else if (ev.type == MM_EVENT_ERROR) {
            handle_status(app, &ev.status);
        } else if (ev.type == MM_EVENT_VERIFY) {
            show_verify(app, ev.transfer.model, ev.result);
        } else if (ev.type == MM_EVENT_EXIT) {
            if (transfer_busy(&app->transfer)) {
                if (strcmp(app->transfer.state, "verifying") == 0) {
                    memset(&app->transfer, 0, sizeof(app->transfer));
                } else {
                    snprintf(app->transfer.state, sizeof(app->transfer.state),
                             "cancelled");
                }
            }
            app->last_state[0] = '\0';
            refresh(app);
        }
    }
}

static gboolean tick(gpointer user) {
    App *app = user;
    app->ticks++;
    if (app->snap.online) {
        mm_transfer t;
        if (mm_progress(&app->backend, &t)) {
            apply_progress(app, &t);
        } else {
            refresh(app);
        }
    } else if (mm_job_running(&app->backend)) {
        pump_job(app);
    } else if (app->ticks % 6 == 0 && mm_online(&app->backend)) {
        /* The voice service started while the window was open. */
        refresh(app);
    }
    if (strcmp(app->transfer.state, "verifying") == 0) {
        size_t i;
        for (i = 0; i < app->cards->len; i++) {
            Card *card = g_ptr_array_index(app->cards, i);
            if (gtk_widget_get_visible(card->progress_box) &&
                strcmp(card->id, app->transfer.model) == 0) {
                gtk_progress_bar_pulse(GTK_PROGRESS_BAR(card->progress));
            }
        }
    }
    return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------ */
/* Cards                                                               */
/* ------------------------------------------------------------------ */

static int is_downloading(const App *app, const voxkit_model_info *row) {
    int transferring = strcmp(app->transfer.model, row->id) == 0 &&
                       transfer_busy(&app->transfer);
    return (transferring && strcmp(app->transfer.state, "downloading") == 0) ||
           (row->downloading && !transferring && app->snap.online);
}

static void status_text(const App *app, const voxkit_model_info *row, char *out,
                        size_t n) {
    int transferring = strcmp(app->transfer.model, row->id) == 0 &&
                       transfer_busy(&app->transfer);
    char size[32];
    if (transferring && strcmp(app->transfer.state, "verifying") == 0) {
        snprintf(out, n, "檢查中…");
    } else if (is_downloading(app, row)) {
        snprintf(out, n, "下載中");
    } else if (row->installed) {
        snprintf(out, n, "%s",
                 mm_row_borrowed(row) ? "已安裝（共用，唯讀）" : "已安裝");
    } else if (row->partial_bytes > 0) {
        mm_format_bytes(size, sizeof(size), row->partial_bytes);
        snprintf(out, n, "已下載 %s，可續傳", size);
    } else {
        snprintf(out, n, "未下載");
    }
}

static void update_card(App *app, Card *card) {
    const voxkit_model_info *row = find_row(app, card->id);
    char text[256];
    int transferring;
    int downloading;
    int busy;
    int consent_given;
    if (!row) {
        return;
    }
    transferring = strcmp(app->transfer.model, row->id) == 0 &&
                   transfer_busy(&app->transfer);
    downloading = is_downloading(app, row);
    busy = someone_busy(app);
    consent_given =
        row->license_accepted || g_hash_table_contains(app->consent, row->id);

    status_text(app, row, text, sizeof(text));
    gtk_label_set_text(GTK_LABEL(card->status), text);

    gtk_widget_set_visible(card->progress_box, transferring || downloading);
    if (transferring && strcmp(app->transfer.state, "verifying") == 0) {
        /* Pulsed by the timer. */
    } else if (transferring || downloading) {
        char got[32];
        char all[32];
        double fraction = 0;
        uint64_t received = transferring ? app->transfer.received : 0;
        uint64_t bytes = transferring ? app->transfer.bytes : row->bytes;
        if (bytes > 0) {
            fraction = (double)received / (double)bytes;
            if (fraction > 1) {
                fraction = 1;
            }
        }
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(card->progress),
                                      fraction);
        mm_format_bytes(got, sizeof(got), received);
        mm_format_bytes(all, sizeof(all), bytes);
        snprintf(text, sizeof(text), "%s / %s", got, all);
        gtk_label_set_text(GTK_LABEL(card->progress_text), text);
    }

    gtk_widget_set_visible(card->cancel, downloading);
    gtk_widget_set_visible(card->download, !downloading && !row->installed);
    gtk_button_set_label(GTK_BUTTON(card->download),
                         row->partial_bytes > 0 ? "繼續下載" : "下載");
    gtk_widget_set_sensitive(card->download, !busy && consent_given);
    gtk_widget_set_visible(card->use,
                           row->installed && row->selectable && !row->active);
    gtk_widget_set_sensitive(card->use, !busy);
    gtk_widget_set_visible(card->verify, row->installed);
    gtk_widget_set_sensitive(card->verify, !busy);
    gtk_widget_set_visible(card->remove, row->installed && row->removable);
    gtk_widget_set_sensitive(card->remove, !transferring);
}

static void update_cards(App *app) {
    size_t i;
    for (i = 0; i < app->cards->len; i++) {
        update_card(app, g_ptr_array_index(app->cards, i));
    }
}

static void on_consent(GtkCheckButton *check, gpointer user) {
    Card *card = user;
    if (gtk_check_button_get_active(check)) {
        g_hash_table_add(card->app->consent, g_strdup(card->id));
    } else {
        g_hash_table_remove(card->app->consent, card->id);
    }
    update_cards(card->app);
}

static void on_download(GtkButton *button, gpointer user) {
    Card *card = user;
    App *app = card->app;
    const voxkit_model_info *row = find_row(app, card->id);
    mm_transfer first;
    mm_status status;
    int accept;
    (void)button;
    if (!row) {
        return;
    }
    accept = row->restricted && g_hash_table_contains(app->consent, row->id);
    show_notice(app, "", 0);
    memset(&status, 0, sizeof(status));
    if (mm_install(&app->backend, row->id, accept, &first, &status) != 0) {
        handle_status(app, &status);
        if (strcmp(status.code, "offline") == 0) {
            refresh(app);
        }
        return;
    }
    if (mm_job_running(&app->backend)) {
        memset(&app->transfer, 0, sizeof(app->transfer));
        snprintf(app->transfer.model, sizeof(app->transfer.model), "%s",
                 row->id);
        snprintf(app->transfer.state, sizeof(app->transfer.state),
                 "downloading");
        app->transfer.received = row->partial_bytes;
        app->transfer.bytes = row->bytes;
        update_cards(app);
    } else {
        apply_progress(app, &first);
    }
}

static void on_cancel(GtkButton *button, gpointer user) {
    Card *card = user;
    App *app = card->app;
    mm_transfer t;
    (void)button;
    if (mm_cancel(&app->backend, &t) == 0 && !mm_job_running(&app->backend) &&
        app->snap.online) {
        apply_progress(app, &t);
    }
}

static void on_use(GtkButton *button, gpointer user) {
    Card *card = user;
    App *app = card->app;
    const voxkit_model_info *row = find_row(app, card->id);
    mm_status status;
    int online = app->snap.online;
    (void)button;
    if (!row) {
        return;
    }
    show_notice(app, "", 0);
    memset(&status, 0, sizeof(status));
    if (mm_select(&app->backend, row->id, &status) == 0) {
        show_notice_f(app, 0,
                      online ? "已改用 %s。"
                             : "已改用 %s（語音服務啟動後生效）。",
                      row->title);
    } else {
        handle_status(app, &status);
    }
    refresh(app);
}

static void on_verify(GtkButton *button, gpointer user) {
    Card *card = user;
    App *app = card->app;
    mm_transfer first;
    mm_status status;
    (void)button;
    show_notice(app, "", 0);
    memset(&status, 0, sizeof(status));
    if (mm_verify(&app->backend, card->id, &first, &status) != 0) {
        handle_status(app, &status);
        return;
    }
    if (mm_job_running(&app->backend)) {
        memset(&app->transfer, 0, sizeof(app->transfer));
        snprintf(app->transfer.model, sizeof(app->transfer.model), "%s",
                 card->id);
        snprintf(app->transfer.state, sizeof(app->transfer.state), "verifying");
        update_cards(app);
    } else {
        apply_progress(app, &first);
    }
}

typedef struct DeleteAsk {
    App *app;
    char id[64];
} DeleteAsk;

static void on_delete_response(AdwAlertDialog *dialog, const char *response,
                               gpointer user) {
    DeleteAsk *ask = user;
    App *app = ask->app;
    (void)dialog;
    if (strcmp(response, "delete") == 0) {
        const voxkit_model_info *row = find_row(app, ask->id);
        mm_status status;
        char title[128];
        snprintf(title, sizeof(title), "%s", row ? row->title : ask->id);
        show_notice(app, "", 0);
        memset(&status, 0, sizeof(status));
        if (mm_remove(&app->backend, ask->id, &status) == 0) {
            show_notice_f(app, 0, "已刪除 %s。", title);
        } else {
            handle_status(app, &status);
        }
        refresh(app);
    }
    g_free(ask);
}

static void on_remove(GtkButton *button, gpointer user) {
    Card *card = user;
    App *app = card->app;
    const voxkit_model_info *row = find_row(app, card->id);
    DeleteAsk *ask;
    AdwDialog *dialog;
    char heading[200];
    char body[300];
    char size[32];
    (void)button;
    if (!row) {
        return;
    }
    ask = g_new0(DeleteAsk, 1);
    ask->app = app;
    snprintf(ask->id, sizeof(ask->id), "%s", row->id);
    snprintf(heading, sizeof(heading), "刪除 %s？", row->title);
    mm_format_bytes(size, sizeof(size), row->size_on_disk);
    snprintf(body, sizeof(body), "會刪除 %s 的模型檔案，之後需要時可重新下載。",
             size);
    dialog = adw_alert_dialog_new(heading, body);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog), "cancel", "取消",
                                   "delete", "刪除", NULL);
    adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "delete",
                                             ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_signal_connect(dialog, "response", G_CALLBACK(on_delete_response), ask);
    adw_dialog_present(dialog, app->window);
}

static GtkWidget *label(const char *text, const char *css) {
    GtkWidget *w = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(w), 0);
    gtk_label_set_wrap(GTK_LABEL(w), TRUE);
    gtk_label_set_selectable(GTK_LABEL(w), FALSE);
    if (css) {
        gtk_widget_add_css_class(w, css);
    }
    return w;
}

static GtkWidget *badge(const char *text, const char *color_class) {
    GtkWidget *w = gtk_label_new(text);
    gtk_widget_add_css_class(w, "mm-badge");
    gtk_widget_add_css_class(w, color_class);
    gtk_widget_set_valign(w, GTK_ALIGN_CENTER);
    return w;
}

static const char *purpose_label(const char *purpose) {
    if (strcmp(purpose, "asr") == 0) {
        return "語音辨識";
    }
    if (strcmp(purpose, "mt") == 0) {
        return "翻譯";
    }
    if (strcmp(purpose, "vad") == 0) {
        return "語音偵測";
    }
    return purpose;
}

static GtkWidget *action_button(const char *text, GCallback cb, Card *card) {
    GtkWidget *b = gtk_button_new_with_label(text);
    g_signal_connect(b, "clicked", cb, card);
    return b;
}

static GtkWidget *build_card(App *app, const voxkit_model_info *row,
                             Card **out) {
    Card *card = g_new0(Card, 1);
    GtkWidget *frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *title;
    GtkWidget *buttons;
    char text[600];
    char a[32];
    char b[32];
    size_t len;
    card->app = app;
    snprintf(card->id, sizeof(card->id), "%s", row->id);
    gtk_widget_add_css_class(frame, "card");
    gtk_widget_set_margin_bottom(frame, 2);
    {
        GtkWidget *pad = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_margin_top(pad, 12);
        gtk_widget_set_margin_bottom(pad, 12);
        gtk_widget_set_margin_start(pad, 12);
        gtk_widget_set_margin_end(pad, 12);
        gtk_box_append(GTK_BOX(frame), pad);

        title = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_append(GTK_BOX(title), label(row->title, "heading"));
        gtk_box_append(GTK_BOX(title),
                       badge(purpose_label(row->purpose), "accent"));
        if (row->recommended) {
            gtk_box_append(GTK_BOX(title), badge("建議", "success"));
        }
        if (row->active && row->installed) {
            gtk_box_append(GTK_BOX(title), badge("使用中", "accent"));
        }
        card->status = label("", "dim-label");
        gtk_widget_set_hexpand(card->status, TRUE);
        gtk_label_set_xalign(GTK_LABEL(card->status), 1);
        gtk_box_append(GTK_BOX(title), card->status);
        gtk_box_append(GTK_BOX(pad), title);

        mm_format_bytes(a, sizeof(a), row->bytes);
        snprintf(text, sizeof(text), "下載 %s", a);
        if (row->runtime_bytes > 0) {
            mm_format_bytes(a, sizeof(a), row->runtime_bytes);
            len = strlen(text);
            snprintf(text + len, sizeof(text) - len, " · 執行時約 %s 記憶體",
                     a);
        }
        if (row->installed && row->size_on_disk > 0) {
            mm_format_bytes(b, sizeof(b), row->size_on_disk);
            len = strlen(text);
            snprintf(text + len, sizeof(text) - len, " · 佔用 %s", b);
        }
        gtk_box_append(GTK_BOX(pad), label(text, NULL));
        if (row->license[0]) {
            snprintf(text, sizeof(text), "授權：%s", row->license);
            gtk_box_append(GTK_BOX(pad), label(text, "caption"));
        }
        if (row->blurb[0]) {
            GtkWidget *blurb = label(row->blurb, "caption");
            gtk_widget_add_css_class(blurb, "dim-label");
            gtk_box_append(GTK_BOX(pad), blurb);
        }
        if (row->restricted && !row->installed) {
            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
            gtk_box_append(
                GTK_BOX(box),
                label(row->restriction[0]
                          ? row->restriction
                          : "此模型有使用限制，下載前請先閱讀授權條款。",
                      "warning"));
            if (row->license_accepted) {
                GtkWidget *done = label("已同意授權條款。", "caption");
                gtk_widget_add_css_class(done, "dim-label");
                gtk_box_append(GTK_BOX(box), done);
            } else {
                card->consent = gtk_check_button_new_with_label(
                    "我已閱讀並同意上述授權條款");
                gtk_check_button_set_active(
                    GTK_CHECK_BUTTON(card->consent),
                    g_hash_table_contains(app->consent, row->id));
                g_signal_connect(card->consent, "toggled",
                                 G_CALLBACK(on_consent), card);
                gtk_box_append(GTK_BOX(box), card->consent);
            }
            gtk_box_append(GTK_BOX(pad), box);
        }

        card->progress_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        card->progress = gtk_progress_bar_new();
        card->progress_text = label("", "caption");
        gtk_widget_add_css_class(card->progress_text, "dim-label");
        gtk_box_append(GTK_BOX(card->progress_box), card->progress);
        gtk_box_append(GTK_BOX(card->progress_box), card->progress_text);
        gtk_widget_set_visible(card->progress_box, FALSE);
        gtk_box_append(GTK_BOX(pad), card->progress_box);

        buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        card->cancel = action_button("取消下載", G_CALLBACK(on_cancel), card);
        card->download = action_button("下載", G_CALLBACK(on_download), card);
        card->use = action_button("使用", G_CALLBACK(on_use), card);
        card->verify = action_button("檢查檔案", G_CALLBACK(on_verify), card);
        card->remove = action_button("刪除", G_CALLBACK(on_remove), card);
        gtk_widget_add_css_class(card->download, "suggested-action");
        gtk_widget_add_css_class(card->remove, "destructive-action");
        gtk_box_append(GTK_BOX(buttons), card->cancel);
        gtk_box_append(GTK_BOX(buttons), card->download);
        gtk_box_append(GTK_BOX(buttons), card->use);
        gtk_box_append(GTK_BOX(buttons), card->verify);
        gtk_box_append(GTK_BOX(buttons), card->remove);
        gtk_box_append(GTK_BOX(pad), buttons);
    }
    *out = card;
    return frame;
}

static void clear_box(GtkWidget *box) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(box)) != NULL) {
        gtk_box_remove(GTK_BOX(box), child);
    }
}

static void selected_line(const App *app, int translate, char *out, size_t n) {
    size_t i;
    for (i = 0; i < app->snap.count; i++) {
        const voxkit_model_info *row = &app->snap.rows[i];
        if ((strcmp(row->purpose, "mt") == 0) != (translate != 0) ||
            !row->active) {
            continue;
        }
        if (row->installed) {
            snprintf(out, n, "目前使用：%s", row->title);
        } else {
            snprintf(out, n, "目前選擇：%s（尚未下載）", row->title);
        }
        return;
    }
    snprintf(out, n, translate ? "尚未選擇翻譯模型" : "尚未選擇語音辨識模型");
}

static void fill_section(App *app, int translate, GtkWidget *subtitle,
                         GtkWidget *cards) {
    char text[300];
    size_t i;
    int any = 0;
    selected_line(app, translate, text, sizeof(text));
    gtk_label_set_text(GTK_LABEL(subtitle), text);
    clear_box(cards);
    for (i = 0; i < app->snap.count; i++) {
        const voxkit_model_info *row = &app->snap.rows[i];
        Card *card;
        GtkWidget *widget;
        /* Speech recognition and voice detection together; translation
         * in its own section. */
        if ((strcmp(row->purpose, "mt") == 0) != (translate != 0)) {
            continue;
        }
        widget = build_card(app, row, &card);
        g_ptr_array_add(app->cards, card);
        gtk_box_append(GTK_BOX(cards), widget);
        any = 1;
    }
    if (!any) {
        gtk_box_append(GTK_BOX(cards),
                       label("這個版本沒有這類模型。", "dim-label"));
    }
}

static void rebuild(App *app) {
    g_ptr_array_set_size(app->cards, 0);
    fill_section(app, 0, app->speech_subtitle, app->speech_cards);
    fill_section(app, 1, app->translate_subtitle, app->translate_cards);
    update_cards(app);
}

/* ------------------------------------------------------------------ */
/* Window                                                              */
/* ------------------------------------------------------------------ */

static void refresh(App *app) {
    mm_snapshot snap;
    char err[128];
    char used[32];
    char avail[32];
    char text[200];
    if (mm_list(&app->backend, &snap, err, sizeof(err)) != 0) {
        memset(&app->snap, 0, sizeof(app->snap));
        app->have_snap = 0;
        mm_error_text(strcmp(err, "no-voxkit") == 0 ? "no-voxkit" : "",
                      "讀不到模型列表。", text, sizeof(text));
        show_notice(app, text, 1);
        gtk_label_set_text(GTK_LABEL(app->usage), "");
        gtk_widget_remove_css_class(app->dot, "success");
        gtk_widget_add_css_class(app->dot, "warning");
        gtk_label_set_text(GTK_LABEL(app->service),
                           "語音服務未執行（離線管理）");
        rebuild(app);
        return;
    }
    app->snap = snap;
    app->have_snap = 1;
    mm_format_bytes(used, sizeof(used), snap.store_bytes);
    mm_format_bytes(avail, sizeof(avail), snap.free_bytes);
    snprintf(text, sizeof(text), "已使用 %s · 可用空間 %s", used, avail);
    gtk_label_set_text(GTK_LABEL(app->usage), text);
    gtk_widget_remove_css_class(app->dot, snap.online ? "warning" : "success");
    gtk_widget_add_css_class(app->dot, snap.online ? "success" : "warning");
    gtk_label_set_text(GTK_LABEL(app->service),
                       snap.online ? "語音服務執行中"
                                   : "語音服務未執行（離線管理）");
    if (!snap.online && !mm_job_running(&app->backend) &&
        transfer_busy(&app->transfer)) {
        memset(&app->transfer, 0, sizeof(app->transfer));
    }
    rebuild(app);
}

static void on_refresh(GtkButton *button, gpointer user) {
    (void)button;
    show_notice(user, "", 0);
    refresh(user);
}

static void on_open_store(GtkButton *button, gpointer user) {
    App *app = user;
    char *dir;
    char *uri;
    (void)button;
    if (app->snap.store[0]) {
        dir = g_strdup(app->snap.store);
    } else {
        dir = g_build_filename(g_get_user_data_dir(), "voxkit", "models", NULL);
    }
    g_mkdir_with_parents(dir, 0755);
    uri = g_filename_to_uri(dir, NULL, NULL);
    if (uri) {
        g_app_info_launch_default_for_uri(uri, NULL, NULL);
    }
    g_free(uri);
    g_free(dir);
}

static GtkWidget *section(const char *title, GtkWidget **subtitle,
                          GtkWidget **cards) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *head = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *name = label(title, "title-3");
    *subtitle = label("", "dim-label");
    *cards = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(head), name);
    gtk_box_append(GTK_BOX(head), *subtitle);
    gtk_box_append(GTK_BOX(box), head);
    gtk_box_append(GTK_BOX(box), *cards);
    return box;
}

static void load_css(void) {
    static const char css[] =
        ".mm-badge { font-size: 0.8em; padding: 1px 7px; border-radius: 5px;"
        " background-color: alpha(currentColor, 0.12); }"
        ".mm-badge.accent { color: @accent_color;"
        " background-color: alpha(@accent_bg_color, 0.2); }"
        ".mm-badge.success { color: @success_color;"
        " background-color: alpha(@success_bg_color, 0.2); }"
        ".mm-dot { font-size: 0.9em; }";
    GtkCssProvider *provider = gtk_css_provider_new();
#if GTK_CHECK_VERSION(4, 12, 0)
    gtk_css_provider_load_from_string(provider, css);
#else
    gtk_css_provider_load_from_data(provider, css, -1);
#endif
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void activate(GApplication *gapp, gpointer user) {
    App *app = user;
    GtkWidget *toolbar;
    GtkWidget *header;
    GtkWidget *root;
    GtkWidget *top;
    GtkWidget *scroll;
    GtkWidget *page;
    GtkWidget *button;
    GtkWidget *state;
    if (app->window) {
        gtk_window_present(GTK_WINDOW(app->window));
        refresh(app);
        return;
    }
    load_css();
    app->window = adw_application_window_new(GTK_APPLICATION(gapp));
    gtk_window_set_title(GTK_WINDOW(app->window), "語音模型");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 640, 680);
    gtk_widget_set_size_request(app->window, 520, 440);

    toolbar = adw_toolbar_view_new();
    header = adw_header_bar_new();
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(header),
                                    adw_window_title_new("語音模型", ""));
    button = gtk_button_new_from_icon_name("view-refresh-symbolic");
    gtk_widget_set_tooltip_text(button, "重新整理");
    g_signal_connect(button, "clicked", G_CALLBACK(on_refresh), app);
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), button);
    button = gtk_button_new_from_icon_name("folder-open-symbolic");
    gtk_widget_set_tooltip_text(button, "開啟模型資料夾");
    g_signal_connect(button, "clicked", G_CALLBACK(on_open_store), app);
    adw_header_bar_pack_end(ADW_HEADER_BAR(header), button);
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    top = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(top, 14);
    gtk_widget_set_margin_bottom(top, 14);
    gtk_widget_set_margin_start(top, 16);
    gtk_widget_set_margin_end(top, 16);
    app->usage = label("讀取中…", NULL);
    gtk_box_append(GTK_BOX(top), app->usage);
    state = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    app->dot = gtk_label_new("●");
    gtk_widget_add_css_class(app->dot, "mm-dot");
    gtk_widget_add_css_class(app->dot, "warning");
    app->service = label("", "caption");
    gtk_widget_add_css_class(app->service, "dim-label");
    gtk_box_append(GTK_BOX(state), app->dot);
    gtk_box_append(GTK_BOX(state), app->service);
    gtk_box_append(GTK_BOX(top), state);
    gtk_box_append(GTK_BOX(root), top);
    gtk_box_append(GTK_BOX(root),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
    gtk_widget_set_margin_top(page, 16);
    gtk_widget_set_margin_bottom(page, 16);
    gtk_widget_set_margin_start(page, 16);
    gtk_widget_set_margin_end(page, 16);
    gtk_box_append(GTK_BOX(page), section("語音辨識模型", &app->speech_subtitle,
                                          &app->speech_cards));
    gtk_box_append(GTK_BOX(page), section("翻譯模型", &app->translate_subtitle,
                                          &app->translate_cards));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), page);
    gtk_box_append(GTK_BOX(root), scroll);

    gtk_box_append(GTK_BOX(root),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    app->notice = label("", "dim-label");
    gtk_widget_set_margin_top(app->notice, 8);
    gtk_widget_set_margin_bottom(app->notice, 8);
    gtk_widget_set_margin_start(app->notice, 16);
    gtk_widget_set_margin_end(app->notice, 16);
    gtk_widget_set_visible(app->notice, FALSE);
    gtk_box_append(GTK_BOX(root), app->notice);

    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), root);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(app->window),
                                       toolbar);
    refresh(app);
    app->timer = g_timeout_add(500, tick, app);
    gtk_window_present(GTK_WINDOW(app->window));
}

static void shutdown_app(GApplication *gapp, gpointer user) {
    App *app = user;
    (void)gapp;
    if (app->timer) {
        g_source_remove(app->timer);
        app->timer = 0;
    }
    mm_close(&app->backend);
}

static void free_card(gpointer card) { g_free(card); }

int main(int argc, char **argv) {
    AdwApplication *gapp;
    int status;
    UI = g_new0(App, 1);
    mm_init(&UI->backend);
    UI->consent = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    UI->cards = g_ptr_array_new_with_free_func(free_card);
    gapp = adw_application_new("im.chewing.plus.model-manager",
                               G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gapp, "activate", G_CALLBACK(activate), UI);
    g_signal_connect(gapp, "shutdown", G_CALLBACK(shutdown_app), UI);
    status = g_application_run(G_APPLICATION(gapp), argc, argv);
    g_object_unref(gapp);
    g_ptr_array_free(UI->cards, TRUE);
    g_hash_table_destroy(UI->consent);
    g_free(UI);
    return status;
}
