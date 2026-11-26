/*
 * SPDX-License-Identifier: MIT
 *
 * Toolkit-free half of the 「語音模型」 window: the same job the macOS
 * ModelManager.swift backend does. It talks to the voxkit daemon over the
 * shared C client, and when the daemon is not running it runs the installed
 * `voxkit model ... --json` command instead. Both paths give the same JSON
 * objects, read with the shared protocol parser. No GTK here, so it can be
 * tested without a display.
 */
#ifndef CHEWING_PLUS_MODELS_BACKEND_H_
#define CHEWING_PLUS_MODELS_BACKEND_H_

#include "voxkit_client.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The catalog, as one model.list reply. */
typedef struct mm_snapshot {
    int online; /* 1: read from the daemon, 0: from the offline command */
    char store[512];
    char model[64];    /* selected speech model */
    char model_mt[64]; /* selected translation model */
    uint64_t store_bytes;
    uint64_t free_bytes;
    int has_free_bytes; /* 0: an older daemon omitted it; do not show 0 B */
    voxkit_model_info rows[VOXKIT_MODELS_MAX];
    size_t count;
} mm_snapshot;

/* One model.progress reply. */
typedef struct mm_transfer {
    char model[64];
    char
        state[32]; /* downloading | verifying | done | cancelled | error | "" */
    char message[1024];
    uint64_t received;
    uint64_t bytes;
} mm_transfer;

/* Result of a request: a refusal carries the daemon's code and message. */
typedef struct mm_status {
    int ok;
    char code[64];
    char message[1024];
} mm_status;

typedef enum mm_event_type {
    MM_EVENT_NONE = 0,
    MM_EVENT_PROGRESS, /* `transfer` is filled */
    MM_EVENT_VERIFY,   /* `transfer.model` and `result` are filled */
    MM_EVENT_ERROR,    /* `status` is filled */
    MM_EVENT_EXIT      /* the offline command has finished */
} mm_event_type;

typedef struct mm_event {
    mm_event_type type;
    mm_transfer transfer;
    mm_status status;
    char result[16];
} mm_event;

typedef struct mm_backend {
    voxkit_client client;
    uint64_t req;
    char voxkit_bin[512]; /* "voxkit", or VOXKIT_BIN for tests */
    pid_t job;            /* offline install / verify, 0 when none */
    int job_fd;
    char job_buf[8192];
    size_t job_len;
    int exit_pending; /* an MM_EVENT_EXIT still to deliver */
} mm_backend;

void mm_init(mm_backend *backend);
void mm_close(mm_backend *backend);

/* 1 when the daemon answers (connects when needed). */
int mm_online(mm_backend *backend);

/* Fills `out`. 0 on success. Falls back to the offline command when the
 * daemon is not there; `err` explains a failure of both. */
int mm_list(mm_backend *backend, mm_snapshot *out, char *err, size_t err_n);

/* Online: one round trip, `out` is the daemon's model.progress.
 * Returns 0 when the daemon did not answer. */
int mm_progress(mm_backend *backend, mm_transfer *out);

/* Start a download. `accept` sends the license consent (HY-MT). Online it
 * returns the first progress in `first`; offline it starts the command and
 * mm_job_pump() delivers the rest. */
int mm_install(mm_backend *backend, const char *id, int accept,
               mm_transfer *first, mm_status *status);
int mm_cancel(mm_backend *backend, mm_transfer *out);
int mm_remove(mm_backend *backend, const char *id, mm_status *status);
int mm_select(mm_backend *backend, const char *id, mm_status *status);
/* Online it starts the daemon's re-hash (poll mm_progress). Offline it
 * starts the command and the result arrives as MM_EVENT_VERIFY. */
int mm_verify(mm_backend *backend, const char *id, mm_transfer *first,
              mm_status *status);

/* Offline job. mm_job_pump() reads what is ready without blocking and
 * returns one event, MM_EVENT_NONE when there is nothing yet. */
int mm_job_running(const mm_backend *backend);
void mm_job_cancel(mm_backend *backend);
mm_event_type mm_job_pump(mm_backend *backend, mm_event *event);

/* Text for the window. */
void mm_format_bytes(char *buf, size_t n, uint64_t bytes);
/* Header line: used space and free space, or 「未知」 when free space was
 * not reported (an older daemon, typically after a package upgrade
 * before the user unit is restarted). */
void mm_usage_text(uint64_t store_bytes, uint64_t free_bytes, int has_free,
                   char *out, size_t n);
/* Before running the offline `voxkit` command, fill in VOXKIT_MODELS_DIR,
 * VOXKIT_MODEL and VOXKIT_MT_MODEL the way the user unit does, without
 * overwriting a value already in the environment. `unit_path` NULL uses
 * the installed unit, then the same compiled-in defaults as the package.
 * This way the offline list shows the same selected model as the daemon. */
void mm_apply_daemon_env(const char *unit_path);
/* The notice for a refusal code (license-required, no-space, in-use, ...). */
void mm_error_text(const char *code, const char *message, char *out, size_t n);
/* verify result word -> notice. `corrupt` returns 1 (an error). */
int mm_verify_text(const char *result, const char *title, char *out, size_t n);

/* Fills in what an older daemon leaves out (title, purpose, selectable,
 * removable, license_accepted) so the window can read every row alike. */
void mm_row_normalize(voxkit_model_info *row);
/* The shared read-only copy of a model (not in this user's store). */
int mm_row_borrowed(const voxkit_model_info *row);

#ifdef __cplusplus
}
#endif

#endif /* CHEWING_PLUS_MODELS_BACKEND_H_ */
