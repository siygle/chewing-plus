/*
 * SPDX-License-Identifier: MIT
 *
 * See backend.h.
 */
#define _POSIX_C_SOURCE 200809L

#include "backend.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MM_SOCKET_TIMEOUT_MS 1500
#define MM_CAPTURE_MAX (256 * 1024)
#define MM_CAPTURE_SECONDS 20

static void copy_text(char *dest, size_t n, const char *src) {
    if (n == 0) {
        return;
    }
    snprintf(dest, n, "%s", src ? src : "");
}

void mm_init(mm_backend *backend) {
    const char *bin = getenv("VOXKIT_BIN");
    memset(backend, 0, sizeof(*backend));
    voxkit_client_init(&backend->client);
    backend->req = 100;
    backend->job_fd = -1;
    copy_text(backend->voxkit_bin, sizeof(backend->voxkit_bin),
              bin && bin[0] ? bin : "voxkit");
}

void mm_close(mm_backend *backend) {
    mm_job_cancel(backend);
    if (backend->job_fd >= 0) {
        close(backend->job_fd);
        backend->job_fd = -1;
    }
    voxkit_client_close(&backend->client);
}

int mm_online(mm_backend *backend) {
    if (voxkit_client_fd(&backend->client) >= 0) {
        return 1;
    }
    return voxkit_client_connect(&backend->client, NULL) == 0;
}

/* ------------------------------------------------------------------ */
/* Offline command                                                     */
/* ------------------------------------------------------------------ */

/* argv for `voxkit model ARGS... --json`. */
static int build_argv(const mm_backend *backend, const char *sub,
                      const char *id, int accept, char **argv, size_t cap) {
    size_t n = 0;
    if (cap < 8) {
        return -1;
    }
    argv[n++] = (char *)backend->voxkit_bin;
    argv[n++] = (char *)"model";
    argv[n++] = (char *)sub;
    if (id) {
        argv[n++] = (char *)id;
    }
    if (accept) {
        argv[n++] = (char *)"--accept-license";
    }
    argv[n++] = (char *)"--json";
    argv[n] = NULL;
    return 0;
}

static pid_t spawn(char *const *argv, int *read_fd) {
    int fds[2];
    pid_t pid;
    if (pipe(fds) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        int null = open("/dev/null", O_WRONLY);
        int in = open("/dev/null", O_RDONLY);
        dup2(fds[1], STDOUT_FILENO);
        if (null >= 0) {
            dup2(null, STDERR_FILENO);
        }
        if (in >= 0) {
            dup2(in, STDIN_FILENO);
        }
        close(fds[0]);
        close(fds[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(fds[1]);
    *read_fd = fds[0];
    return pid;
}

/* Runs a short command to the end and returns its stdout. -1 when it cannot
 * run, else the exit code (127: voxkit is not installed). */
static int run_capture(char *const *argv, char *out, size_t cap) {
    int fd = -1;
    size_t len = 0;
    int status = 0;
    int waited = 0;
    pid_t pid = spawn(argv, &fd);
    if (pid < 0) {
        return -1;
    }
    out[0] = '\0';
    for (;;) {
        struct pollfd pfd;
        ssize_t n;
        int ready;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        ready = poll(&pfd, 1, MM_CAPTURE_SECONDS * 1000);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready <= 0) {
            kill(pid, SIGTERM);
            break;
        }
        if (len + 1 >= cap) {
            kill(pid, SIGTERM);
            break;
        }
        n = read(fd, out + len, cap - len - 1);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            break;
        }
        len += (size_t)n;
    }
    out[len] = '\0';
    close(fd);
    while (!waited) {
        if (waitpid(pid, &status, 0) >= 0 || errno != EINTR) {
            waited = 1;
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

typedef struct offline_lines {
    char *text;
    char *cursor;
} offline_lines;

static char *next_line(offline_lines *lines) {
    char *start = lines->cursor;
    char *nl;
    if (!start || !*start) {
        return NULL;
    }
    nl = strchr(start, '\n');
    if (nl) {
        *nl = '\0';
        lines->cursor = nl + 1;
    } else {
        lines->cursor = start + strlen(start);
    }
    return start;
}

static void status_set_error(mm_status *status, const char *code,
                             const char *message) {
    status->ok = 0;
    copy_text(status->code, sizeof(status->code), code);
    copy_text(status->message, sizeof(status->message), message);
}

static void status_set_ok(mm_status *status) {
    status->ok = 1;
    status->code[0] = '\0';
    status->message[0] = '\0';
}

/* The error object of a refused request. */
static void status_from_message(mm_status *status, const voxkit_message *msg,
                                const char *err) {
    status_set_error(status, msg->code,
                     msg->message[0] ? msg->message : (err ? err : ""));
}

/* Runs `voxkit model SUB [ID] --json` and reads every line. `want` is the
 * type that means success. Returns 0 when it succeeded. */
static int offline_simple(mm_backend *backend, const char *sub, const char *id,
                          voxkit_msg_type want, mm_status *status) {
    char *argv[8];
    char *buf = malloc(MM_CAPTURE_MAX);
    offline_lines lines;
    char *line;
    int rc;
    int ok = -1;
    if (!buf) {
        status_set_error(status, "io", "out of memory");
        return -1;
    }
    build_argv(backend, sub, id, 0, argv, 8);
    rc = run_capture(argv, buf, MM_CAPTURE_MAX);
    if (rc < 0 || rc == 127) {
        status_set_error(status, "no-voxkit", "voxkit is not installed");
        free(buf);
        return -1;
    }
    lines.text = buf;
    lines.cursor = buf;
    status_set_error(status, "failed", "voxkit gave no answer");
    while ((line = next_line(&lines)) != NULL) {
        voxkit_message msg;
        memset(&msg, 0, sizeof(msg));
        if (voxkit_proto_parse(line, &msg, NULL, 0) != VOXKIT_PROTO_OK) {
            continue;
        }
        if (msg.type == VOXKIT_MSG_ERROR) {
            status_from_message(status, &msg, NULL);
            ok = -1;
            break;
        }
        if (msg.type == want) {
            status_set_ok(status);
            ok = 0;
            break;
        }
    }
    free(buf);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Catalog                                                             */
/* ------------------------------------------------------------------ */

int mm_row_borrowed(const voxkit_model_info *row) {
    return row->source[0] && strcmp(row->source, "store") != 0;
}

void mm_row_normalize(voxkit_model_info *row) {
    if (!row->title[0]) {
        copy_text(row->title, sizeof(row->title), row->id);
    }
    if (!row->purpose[0]) {
        copy_text(row->purpose, sizeof(row->purpose), row->kind);
    }
    if (!row->has_selectable) {
        row->selectable = strcmp(row->purpose, "vad") != 0;
    }
    if (!row->has_removable) {
        row->removable = row->installed;
    }
    if (!row->restricted) {
        row->license_accepted = 1;
    }
}

static void fill_snapshot(mm_snapshot *out, const char *line, int online,
                          char *err, size_t err_n) {
    voxkit_message summary;
    size_t i;
    memset(&summary, 0, sizeof(summary));
    out->online = online;
    out->count = 0;
    voxkit_proto_parse(line, &summary, NULL, 0);
    voxkit_proto_parse_models(line, out->rows, VOXKIT_MODELS_MAX, &out->count,
                              err, err_n);
    for (i = 0; i < out->count; i++) {
        mm_row_normalize(&out->rows[i]);
    }
    copy_text(out->store, sizeof(out->store), summary.store);
    copy_text(out->model, sizeof(out->model), summary.model);
    copy_text(out->model_mt, sizeof(out->model_mt), summary.model_mt);
    out->store_bytes = summary.store_bytes;
    out->free_bytes = summary.free_bytes;
}

static int list_online(mm_backend *backend, mm_snapshot *out) {
    voxkit_model_info rows[VOXKIT_MODELS_MAX];
    voxkit_message summary;
    size_t count = 0;
    char err[128] = "";
    int rc;
    if (!mm_online(backend)) {
        return -1;
    }
    memset(&summary, 0, sizeof(summary));
    rc = voxkit_client_model_list(&backend->client, ++backend->req, rows,
                                  VOXKIT_MODELS_MAX, &count, &summary,
                                  MM_SOCKET_TIMEOUT_MS, err, sizeof(err));
    /* More rows than room still filled the first VOXKIT_MODELS_MAX. */
    if (rc != 0 && rc != VOXKIT_PROTO_ERR_RANGE) {
        /* The daemon went away or spoke something this client cannot read. */
        voxkit_client_close(&backend->client);
        return -1;
    }
    fill_snapshot(out, voxkit_client_last_line(&backend->client), 1, NULL, 0);
    return 0;
}

static int list_offline(mm_backend *backend, mm_snapshot *out, char *err,
                        size_t err_n) {
    char *argv[8];
    char *buf = malloc(MM_CAPTURE_MAX);
    offline_lines lines;
    char *line;
    int rc;
    int found = 0;
    if (!buf) {
        copy_text(err, err_n, "out of memory");
        return -1;
    }
    build_argv(backend, "list", NULL, 0, argv, 8);
    rc = run_capture(argv, buf, MM_CAPTURE_MAX);
    if (rc < 0 || rc == 127) {
        copy_text(err, err_n, "no-voxkit");
        free(buf);
        return -1;
    }
    lines.text = buf;
    lines.cursor = buf;
    while ((line = next_line(&lines)) != NULL) {
        voxkit_message msg;
        memset(&msg, 0, sizeof(msg));
        if (voxkit_proto_parse(line, &msg, NULL, 0) == VOXKIT_PROTO_OK &&
            msg.type == VOXKIT_MSG_MODEL_LIST) {
            fill_snapshot(out, line, 0, NULL, 0);
            found = 1;
            break;
        }
    }
    free(buf);
    if (!found) {
        copy_text(err, err_n, "no-list");
        return -1;
    }
    return 0;
}

int mm_list(mm_backend *backend, mm_snapshot *out, char *err, size_t err_n) {
    memset(out, 0, sizeof(*out));
    if (err && err_n) {
        err[0] = '\0';
    }
    if (list_online(backend, out) == 0) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    return list_offline(backend, out, err, err_n);
}

/* ------------------------------------------------------------------ */
/* Requests                                                            */
/* ------------------------------------------------------------------ */

static void transfer_from(mm_transfer *out, const voxkit_message *msg) {
    copy_text(out->model, sizeof(out->model), msg->model);
    copy_text(out->state, sizeof(out->state), msg->state);
    copy_text(out->message, sizeof(out->message), msg->message);
    out->received = msg->received;
    out->bytes = msg->bytes;
}

int mm_progress(mm_backend *backend, mm_transfer *out) {
    voxkit_message msg;
    memset(&msg, 0, sizeof(msg));
    memset(out, 0, sizeof(*out));
    if (voxkit_client_fd(&backend->client) < 0) {
        return 0;
    }
    if (voxkit_client_model_progress(&backend->client, ++backend->req, &msg,
                                     300, NULL, 0) != 0) {
        return 0;
    }
    transfer_from(out, &msg);
    return 1;
}

static void online_failed(mm_backend *backend, const voxkit_message *msg,
                          const char *err, mm_status *status) {
    if (voxkit_client_fd(&backend->client) < 0) {
        status_set_error(status, "offline", "voxkit is not running");
    } else {
        status_from_message(status, msg, err);
    }
}

static int start_job(mm_backend *backend, const char *sub, const char *id,
                     int accept, mm_status *status) {
    char *argv[8];
    int fd = -1;
    pid_t pid;
    int flags;
    if (backend->job > 0) {
        status_set_error(status, "busy", "another download is running");
        return -1;
    }
    build_argv(backend, sub, id, accept, argv, 8);
    pid = spawn(argv, &fd);
    if (pid < 0) {
        status_set_error(status, "io", "cannot start voxkit");
        return -1;
    }
    flags = fcntl(fd, F_GETFL);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    backend->job = pid;
    backend->job_fd = fd;
    backend->job_len = 0;
    backend->exit_pending = 0;
    status_set_ok(status);
    return 0;
}

int mm_install(mm_backend *backend, const char *id, int accept,
               mm_transfer *first, mm_status *status) {
    voxkit_message msg;
    char err[256] = "";
    int rc;
    memset(&msg, 0, sizeof(msg));
    memset(first, 0, sizeof(*first));
    if (!mm_online(backend)) {
        return start_job(backend, "install", id, accept, status);
    }
    rc = accept ? voxkit_client_model_install_accept(
                      &backend->client, ++backend->req, id, &msg,
                      MM_SOCKET_TIMEOUT_MS, err, sizeof(err))
                : voxkit_client_model_install(&backend->client, ++backend->req,
                                              id, &msg, MM_SOCKET_TIMEOUT_MS,
                                              err, sizeof(err));
    if (rc != 0) {
        online_failed(backend, &msg, err, status);
        return -1;
    }
    transfer_from(first, &msg);
    status_set_ok(status);
    return 0;
}

int mm_cancel(mm_backend *backend, mm_transfer *out) {
    voxkit_message msg;
    memset(&msg, 0, sizeof(msg));
    memset(out, 0, sizeof(*out));
    if (backend->job > 0) {
        mm_job_cancel(backend);
        return 0;
    }
    if (voxkit_client_fd(&backend->client) < 0) {
        return -1;
    }
    if (voxkit_client_model_cancel(&backend->client, ++backend->req, &msg,
                                   MM_SOCKET_TIMEOUT_MS, NULL, 0) != 0) {
        return -1;
    }
    transfer_from(out, &msg);
    return 0;
}

int mm_remove(mm_backend *backend, const char *id, mm_status *status) {
    voxkit_message msg;
    char err[256] = "";
    memset(&msg, 0, sizeof(msg));
    if (!mm_online(backend)) {
        return offline_simple(backend, "remove", id, VOXKIT_MSG_MODEL_REMOVE,
                              status);
    }
    if (voxkit_client_model_remove(&backend->client, ++backend->req, id, &msg,
                                   MM_SOCKET_TIMEOUT_MS, err,
                                   sizeof(err)) != 0) {
        online_failed(backend, &msg, err, status);
        return -1;
    }
    status_set_ok(status);
    return 0;
}

int mm_select(mm_backend *backend, const char *id, mm_status *status) {
    voxkit_message msg;
    char err[256] = "";
    memset(&msg, 0, sizeof(msg));
    if (!mm_online(backend)) {
        return offline_simple(backend, "select", id, VOXKIT_MSG_MODEL_SELECT,
                              status);
    }
    if (voxkit_client_model_select(&backend->client, ++backend->req, id, &msg,
                                   MM_SOCKET_TIMEOUT_MS, err,
                                   sizeof(err)) != 0) {
        online_failed(backend, &msg, err, status);
        return -1;
    }
    status_set_ok(status);
    return 0;
}

int mm_verify(mm_backend *backend, const char *id, mm_transfer *first,
              mm_status *status) {
    voxkit_message msg;
    char err[256] = "";
    memset(&msg, 0, sizeof(msg));
    memset(first, 0, sizeof(*first));
    if (!mm_online(backend)) {
        return start_job(backend, "verify", id, 0, status);
    }
    if (voxkit_client_model_verify(&backend->client, ++backend->req, id, &msg,
                                   MM_SOCKET_TIMEOUT_MS, err,
                                   sizeof(err)) != 0) {
        online_failed(backend, &msg, err, status);
        return -1;
    }
    transfer_from(first, &msg);
    status_set_ok(status);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Offline job                                                         */
/* ------------------------------------------------------------------ */

int mm_job_running(const mm_backend *backend) {
    return backend->job > 0 || backend->exit_pending;
}

void mm_job_cancel(mm_backend *backend) {
    if (backend->job > 0) {
        /* The partial file stays for a later resume. */
        kill(backend->job, SIGTERM);
    }
}

static void finish_job(mm_backend *backend, mm_event *event) {
    int status = 0;
    pid_t pid = backend->job;
    close(backend->job_fd);
    backend->job_fd = -1;
    backend->job = 0;
    backend->job_len = 0;
    backend->exit_pending = 1;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        event->type = MM_EVENT_ERROR;
        status_set_error(&event->status, "no-voxkit",
                         "voxkit is not installed");
    }
}

static int take_job_line(mm_backend *backend, mm_event *event) {
    char *nl = memchr(backend->job_buf, '\n', backend->job_len);
    char line[8192];
    size_t len;
    voxkit_message msg;
    if (!nl) {
        return 0;
    }
    len = (size_t)(nl - backend->job_buf);
    if (len >= sizeof(line)) {
        len = sizeof(line) - 1;
    }
    memcpy(line, backend->job_buf, len);
    line[len] = '\0';
    backend->job_len -= (size_t)(nl - backend->job_buf) + 1;
    memmove(backend->job_buf, nl + 1, backend->job_len);
    memset(&msg, 0, sizeof(msg));
    if (voxkit_proto_parse(line, &msg, NULL, 0) != VOXKIT_PROTO_OK) {
        return 0;
    }
    if (msg.type == VOXKIT_MSG_MODEL_PROGRESS) {
        event->type = MM_EVENT_PROGRESS;
        transfer_from(&event->transfer, &msg);
        return 1;
    }
    if (msg.type == VOXKIT_MSG_ERROR) {
        event->type = MM_EVENT_ERROR;
        status_from_message(&event->status, &msg, NULL);
        return 1;
    }
    if (msg.type == VOXKIT_MSG_MODEL_VERIFY) {
        event->type = MM_EVENT_VERIFY;
        copy_text(event->transfer.model, sizeof(event->transfer.model),
                  msg.model);
        copy_text(event->result, sizeof(event->result), msg.result);
        return 1;
    }
    return 0;
}

mm_event_type mm_job_pump(mm_backend *backend, mm_event *event) {
    memset(event, 0, sizeof(*event));
    for (;;) {
        ssize_t n;
        if (take_job_line(backend, event)) {
            return event->type;
        }
        if (backend->job <= 0) {
            break;
        }
        if (backend->job_len + 1 >= sizeof(backend->job_buf)) {
            /* A line longer than any reply; drop it. */
            backend->job_len = 0;
        }
        n = read(backend->job_fd, backend->job_buf + backend->job_len,
                 sizeof(backend->job_buf) - backend->job_len - 1);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return MM_EVENT_NONE;
        }
        if (n <= 0) {
            finish_job(backend, event);
            if (event->type != MM_EVENT_NONE) {
                return event->type;
            }
            break;
        }
        backend->job_len += (size_t)n;
    }
    if (backend->exit_pending) {
        backend->exit_pending = 0;
        event->type = MM_EVENT_EXIT;
        return MM_EVENT_EXIT;
    }
    return MM_EVENT_NONE;
}

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

void mm_format_bytes(char *buf, size_t n, uint64_t bytes) {
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

void mm_error_text(const char *code, const char *message, char *out, size_t n) {
    const char *msg = message ? message : "";
    if (strcmp(code, "license-required") == 0) {
        copy_text(out, n, "請先勾選同意授權條款再下載。");
    } else if (strcmp(code, "no-space") == 0) {
        snprintf(out, n, "磁碟空間不足：%s", msg);
    } else if (strcmp(code, "in-use") == 0) {
        copy_text(out, n, "模型正在使用中，請稍後再刪除。");
    } else if (strcmp(code, "daemon-running") == 0) {
        copy_text(out, n, "語音服務剛啟動，請按「重新整理」。");
    } else if (strcmp(code, "no-voxkit") == 0) {
        copy_text(out, n, "找不到 voxkit。請重新安裝新酷音 Plus。");
    } else if (strcmp(code, "offline") == 0) {
        copy_text(out, n, "連不到語音服務。");
    } else if (strcmp(code, "busy") == 0) {
        copy_text(out, n, "已有下載進行中。");
    } else {
        copy_text(out, n, msg[0] ? msg : (code[0] ? code : "失敗。"));
    }
}

int mm_verify_text(const char *result, const char *title, char *out, size_t n) {
    if (strcmp(result, "ok") == 0) {
        snprintf(out, n, "%s 檢查通過。", title);
        return 0;
    }
    if (strcmp(result, "missing") == 0) {
        snprintf(out, n, "%s 尚未下載。", title);
    } else if (strcmp(result, "corrupt") == 0) {
        snprintf(out, n, "%s 檔案損毀，請刪除後重新下載。", title);
    } else {
        copy_text(out, n, result);
    }
    return 1;
}
