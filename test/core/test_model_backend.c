/*
 * SPDX-License-Identifier: MIT
 *
 * The model window's backend with no display: a fake voxkit daemon on a
 * Unix socket (online path) and a fake `voxkit` command (offline path).
 */
#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L

#include "backend.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static int failures = 0;

static void check(int ok, const char *what) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

static void pause_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

#define LIST_BODY                                                              \
    "\"daemon\":\"%s\",\"store\":\"/tmp/fake-models\","                        \
    "\"store_bytes\":1500,\"free_bytes\":9000000000,"                          \
    "\"model\":\"qwen3-asr-0.6b\",\"model_mt\":\"hy-mt1.5-1.8b\","             \
    "\"models\":["                                                             \
    "{\"id\":\"qwen3-asr-0.6b\",\"kind\":\"asr\",\"purpose\":\"asr\","         \
    "\"title\":\"Qwen3-ASR 0.6B\",\"bytes\":900000000,"                        \
    "\"size_on_disk\":900000000,\"partial_bytes\":0,\"installed\":true,"       \
    "\"source\":\"store\",\"removable\":true,\"active\":true,"                 \
    "\"selectable\":true,\"downloading\":false,\"recommended\":true,"          \
    "\"runtime_bytes\":260000000,\"license\":\"Apache-2.0\","                  \
    "\"restricted\":false,\"blurb\":\"speech\"},"                              \
    "{\"id\":\"silero-vad\",\"kind\":\"vad\",\"purpose\":\"vad\","             \
    "\"title\":\"Silero VAD\",\"bytes\":2000000,\"installed\":true,"           \
    "\"source\":\"read-only\",\"removable\":false,\"active\":false,"           \
    "\"selectable\":false,\"restricted\":false},"                              \
    "{\"id\":\"hy-mt1.5-1.8b\",\"kind\":\"mt\",\"purpose\":\"mt\","            \
    "\"title\":\"HY-MT1.5 1.8B\",\"bytes\":1133080512,"                        \
    "\"partial_bytes\":500000000,\"installed\":false,\"active\":true,"         \
    "\"selectable\":true,\"license\":\"Tencent HY\",\"restricted\":true,"      \
    "\"license_accepted\":false,"                                              \
    "\"restriction\":\"not for use in the EU\"}]"

static const char *accepted_id = "";

static void reply(int fd, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#include <stdarg.h>
static void reply(int fd, const char *fmt, ...) {
    char buf[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    send(fd, buf, strlen(buf), MSG_NOSIGNAL);
}

static int downloading_state = 0;

static void serve_line(int fd, const char *line) {
    voxkit_message msg;
    memset(&msg, 0, sizeof(msg));
    if (voxkit_proto_parse(line, &msg, NULL, 0) != VOXKIT_PROTO_OK) {
        return;
    }
    if (msg.type == VOXKIT_MSG_MODEL_LIST) {
        reply(fd,
              "{\"v\":1,\"type\":\"model.list\",\"req\":%llu," LIST_BODY "}\n",
              (unsigned long long)msg.req, "running");
    } else if (msg.type == VOXKIT_MSG_MODEL_INSTALL) {
        if (strcmp(msg.model, "hy-mt1.5-1.8b") == 0 &&
            !strstr(line, "\"accept_license\":true")) {
            reply(fd,
                  "{\"v\":1,\"type\":\"error\",\"req\":%llu,"
                  "\"code\":\"license-required\",\"message\":\"accept\"}\n",
                  (unsigned long long)msg.req);
        } else if (strcmp(msg.model, "huge") == 0) {
            reply(fd,
                  "{\"v\":1,\"type\":\"error\",\"req\":%llu,"
                  "\"code\":\"no-space\",\"message\":\"need 2 GB\"}\n",
                  (unsigned long long)msg.req);
        } else {
            accepted_id = strstr(line, "accept_license") ? "yes" : "no";
            downloading_state = 1;
            reply(fd,
                  "{\"v\":1,\"type\":\"model.progress\",\"req\":%llu,"
                  "\"state\":\"downloading\",\"model\":\"%s\","
                  "\"received\":10,\"bytes\":100,\"message\":\"\"}\n",
                  (unsigned long long)msg.req, msg.model);
        }
    } else if (msg.type == VOXKIT_MSG_MODEL_PROGRESS) {
        reply(fd,
              "{\"v\":1,\"type\":\"model.progress\",\"req\":%llu,"
              "\"state\":\"%s\",\"model\":\"hy-mt1.5-1.8b\","
              "\"received\":%d,\"bytes\":100,\"message\":\"\"}\n",
              (unsigned long long)msg.req,
              downloading_state ? "downloading" : "idle",
              downloading_state ? 55 : 0);
    } else if (msg.type == VOXKIT_MSG_MODEL_CANCEL) {
        downloading_state = 0;
        reply(fd,
              "{\"v\":1,\"type\":\"model.progress\",\"req\":%llu,"
              "\"state\":\"cancelled\",\"model\":\"hy-mt1.5-1.8b\","
              "\"received\":55,\"bytes\":100,\"message\":\"\"}\n",
              (unsigned long long)msg.req);
    } else if (msg.type == VOXKIT_MSG_MODEL_REMOVE) {
        if (strcmp(msg.model, "qwen3-asr-0.6b") == 0) {
            reply(fd,
                  "{\"v\":1,\"type\":\"error\",\"req\":%llu,"
                  "\"code\":\"in-use\",\"message\":\"busy\"}\n",
                  (unsigned long long)msg.req);
        } else {
            reply(fd,
                  "{\"v\":1,\"type\":\"model.remove\",\"req\":%llu,"
                  "\"model\":\"%s\",\"installed\":false}\n",
                  (unsigned long long)msg.req, msg.model);
        }
    } else if (msg.type == VOXKIT_MSG_MODEL_SELECT) {
        reply(fd,
              "{\"v\":1,\"type\":\"model.select\",\"req\":%llu,"
              "\"model\":\"%s\"}\n",
              (unsigned long long)msg.req, msg.model);
    } else if (msg.type == VOXKIT_MSG_MODEL_VERIFY) {
        reply(fd,
              "{\"v\":1,\"type\":\"model.progress\",\"req\":%llu,"
              "\"state\":\"verifying\",\"model\":\"%s\",\"received\":0,"
              "\"bytes\":0,\"message\":\"\"}\n",
              (unsigned long long)msg.req, msg.model);
    }
}

typedef struct server {
    int listener;
    volatile int stop;
} server;

static void *serve(void *arg) {
    server *srv = arg;
    while (!srv->stop) {
        int fd = accept(srv->listener, NULL, NULL);
        char buf[4096];
        size_t len = 0;
        if (fd < 0) {
            break;
        }
        for (;;) {
            ssize_t n = recv(fd, buf + len, sizeof(buf) - 1 - len, 0);
            char *nl;
            if (n <= 0) {
                break;
            }
            len += (size_t)n;
            buf[len] = '\0';
            while ((nl = strchr(buf, '\n')) != NULL) {
                *nl = '\0';
                serve_line(fd, buf);
                memmove(buf, nl + 1, strlen(nl + 1) + 1);
                len = strlen(buf);
            }
        }
        close(fd);
    }
    return NULL;
}

static const char kFakeVoxkit[] =
    "#!/bin/sh\n"
    "sub=$2\n"
    "id=$3\n"
    "case \"$sub\" in\n"
    "list)\n"
    "  echo '{\"v\":1,\"type\":\"model.list\",\"req\":0," LIST_BODY "}'\n"
    "  ;;\n"
    "install)\n"
    "  case \" $* \" in *\" --accept-license \"*) ok=1;; *) ok=0;; esac\n"
    "  if [ \"$id\" = hy-mt1.5-1.8b ] && [ $ok = 0 ]; then\n"
    "    echo '{\"v\":1,\"type\":\"error\",\"req\":0,"
    "\"code\":\"license-required\",\"message\":\"accept the license\"}'\n"
    "    exit 1\n"
    "  fi\n"
    "  echo '{\"v\":1,\"type\":\"model.progress\",\"req\":0,"
    "\"state\":\"downloading\",\"model\":\"'$id'\",\"received\":10,"
    "\"bytes\":100,\"message\":\"\"}'\n"
    "  if [ \"$id\" = slow ]; then exec sleep 30; fi\n"
    "  echo '{\"v\":1,\"type\":\"model.progress\",\"req\":0,"
    "\"state\":\"done\",\"model\":\"'$id'\",\"received\":100,"
    "\"bytes\":100,\"message\":\"\"}'\n"
    "  ;;\n"
    "verify)\n"
    "  echo '{\"v\":1,\"type\":\"model.verify\",\"req\":0,"
    "\"model\":\"'$id'\",\"result\":\"corrupt\"}'\n"
    "  exit 1\n"
    "  ;;\n"
    "remove)\n"
    "  if [ \"$id\" = running ]; then\n"
    "    echo '{\"v\":1,\"type\":\"error\",\"req\":0,"
    "\"code\":\"daemon-running\",\"message\":\"stop it\"}'\n"
    "    exit 1\n"
    "  fi\n"
    "  echo '{\"v\":1,\"type\":\"model.remove\",\"req\":0,"
    "\"model\":\"'$id'\",\"installed\":false}'\n"
    "  ;;\n"
    "select)\n"
    "  echo '{\"v\":1,\"type\":\"model.select\",\"req\":0,"
    "\"model\":\"'$id'\"}'\n"
    "  ;;\n"
    "esac\n";

/* Pumps the offline job until it exits (or 5 s). */
static int drain(mm_backend *be, mm_event *events, int cap) {
    int n = 0;
    int spins;
    for (spins = 0; spins < 250; spins++) {
        mm_event ev;
        mm_event_type type;
        while ((type = mm_job_pump(be, &ev)) != MM_EVENT_NONE) {
            if (n < cap) {
                events[n++] = ev;
            }
            if (type == MM_EVENT_EXIT) {
                return n;
            }
        }
        pause_ms(20);
    }
    return n;
}

static void test_protocol_fields(void) {
    voxkit_model_info rows[4];
    size_t count = 0;
    const char *line =
        "{\"v\":1,\"type\":\"model.list\",\"models\":[{\"id\":\"a\","
        "\"kind\":\"asr\",\"installed\":true,\"source\":\"read-only\","
        "\"removable\":false,\"selectable\":true},{\"id\":\"b\","
        "\"kind\":\"vad\",\"installed\":true}]}";
    check(voxkit_proto_parse_models(line, rows, 4, &count, NULL, 0) ==
              VOXKIT_PROTO_OK,
          "parse list with the source / removable keys");
    check(count == 2 && strcmp(rows[0].source, "read-only") == 0 &&
              rows[0].has_removable && !rows[0].removable &&
              rows[0].has_selectable && rows[0].selectable,
          "source, removable and selectable are read");
    check(mm_row_borrowed(&rows[0]) && !mm_row_borrowed(&rows[1]),
          "a read-only copy is borrowed, a missing source is not");
    mm_row_normalize(&rows[1]);
    check(strcmp(rows[1].title, "b") == 0 &&
              strcmp(rows[1].purpose, "vad") == 0 && !rows[1].selectable &&
              rows[1].removable && rows[1].license_accepted,
          "an older daemon's row gets title, purpose, selectable (not for "
          "vad), removable and license_accepted filled in");
}

static void check_snapshot(const mm_snapshot *snap, int online,
                           const char *tag) {
    char what[128];
    snprintf(what, sizeof(what), "%s: three models, online=%d", tag, online);
    check(snap->count == 3 && snap->online == online, what);
    snprintf(what, sizeof(what), "%s: store, used and free space", tag);
    check(strcmp(snap->store, "/tmp/fake-models") == 0 &&
              snap->store_bytes == 1500 && snap->free_bytes == 9000000000ULL &&
              snap->has_free_bytes,
          what);
    snprintf(what, sizeof(what), "%s: selected speech and translation model",
             tag);
    check(strcmp(snap->model, "qwen3-asr-0.6b") == 0 &&
              strcmp(snap->model_mt, "hy-mt1.5-1.8b") == 0,
          what);
    snprintf(what, sizeof(what), "%s: card fields", tag);
    check(strcmp(snap->rows[0].title, "Qwen3-ASR 0.6B") == 0 &&
              snap->rows[0].active && snap->rows[0].recommended &&
              snap->rows[0].runtime_bytes == 260000000ULL &&
              snap->rows[1].selectable == 0 &&
              mm_row_borrowed(&snap->rows[1]) && !snap->rows[1].removable &&
              snap->rows[2].restricted && !snap->rows[2].license_accepted &&
              snap->rows[2].partial_bytes == 500000000ULL &&
              strcmp(snap->rows[2].purpose, "mt") == 0,
          what);
}

static void test_offline(const char *dir) {
    mm_backend be;
    mm_snapshot snap;
    mm_status st;
    mm_transfer first;
    mm_event events[16];
    char err[128];
    int n;
    char sock[256];
    snprintf(sock, sizeof(sock), "%s/none.sock", dir);
    setenv("VOXKIT_SOCKET", sock, 1);
    mm_init(&be);

    check(!mm_online(&be), "offline: no daemon on the socket");
    check(mm_list(&be, &snap, err, sizeof(err)) == 0, "offline: list");
    check_snapshot(&snap, 0, "offline");

    memset(&st, 0, sizeof(st));
    check(mm_install(&be, "hy-mt1.5-1.8b", 0, &first, &st) == 0 &&
              mm_job_running(&be),
          "offline: install starts a job");
    n = drain(&be, events, 16);
    check(n >= 2 && events[0].type == MM_EVENT_ERROR &&
              strcmp(events[0].status.code, "license-required") == 0 &&
              events[n - 1].type == MM_EVENT_EXIT,
          "offline: a restricted model without consent is refused");
    check(!mm_job_running(&be), "offline: the job is finished");

    check(mm_install(&be, "hy-mt1.5-1.8b", 1, &first, &st) == 0,
          "offline: install with consent starts");
    n = drain(&be, events, 16);
    check(n == 3 && events[0].type == MM_EVENT_PROGRESS &&
              strcmp(events[0].transfer.state, "downloading") == 0 &&
              events[0].transfer.received == 10 &&
              events[0].transfer.bytes == 100 &&
              events[1].type == MM_EVENT_PROGRESS &&
              strcmp(events[1].transfer.state, "done") == 0 &&
              events[2].type == MM_EVENT_EXIT,
          "offline: progress, done, exit");

    check(mm_install(&be, "slow", 0, &first, &st) == 0, "offline: slow job");
    check(mm_install(&be, "other", 0, &first, &st) != 0 &&
              strcmp(st.code, "busy") == 0,
          "offline: a second download is refused while one runs");
    pause_ms(100);
    {
        mm_transfer t;
        mm_event ev;
        check(mm_cancel(&be, &t) == 0, "offline: cancel");
        n = drain(&be, events, 16);
        (void)ev;
        check(n >= 1 && events[n - 1].type == MM_EVENT_EXIT &&
                  !mm_job_running(&be),
              "offline: cancel ends the job");
    }

    check(mm_verify(&be, "qwen3-asr-0.6b", &first, &st) == 0,
          "offline: verify starts");
    n = drain(&be, events, 16);
    check(n == 2 && events[0].type == MM_EVENT_VERIFY &&
              strcmp(events[0].result, "corrupt") == 0 &&
              strcmp(events[0].transfer.model, "qwen3-asr-0.6b") == 0,
          "offline: verify result");

    check(mm_select(&be, "hy-mt1.5-1.8b", &st) == 0 && st.ok,
          "offline: select");
    check(mm_remove(&be, "silero-vad", &st) == 0 && st.ok, "offline: remove");
    check(mm_remove(&be, "running", &st) != 0 &&
              strcmp(st.code, "daemon-running") == 0,
          "offline: remove is refused while the daemon is running");
    mm_close(&be);

    setenv("VOXKIT_BIN", "/nonexistent/voxkit", 1);
    mm_init(&be);
    check(mm_list(&be, &snap, err, sizeof(err)) != 0 &&
              strcmp(err, "no-voxkit") == 0,
          "no voxkit installed: list reports it");
    check(mm_remove(&be, "x", &st) != 0 && strcmp(st.code, "no-voxkit") == 0,
          "no voxkit installed: remove reports it");
    check(mm_install(&be, "x", 0, &first, &st) == 0, "missing binary: spawn");
    n = drain(&be, events, 16);
    check(n == 2 && events[0].type == MM_EVENT_ERROR &&
              strcmp(events[0].status.code, "no-voxkit") == 0,
          "no voxkit installed: the job reports it, then exits");
    mm_close(&be);
}

static void test_online(const char *dir) {
    mm_backend be;
    mm_snapshot snap;
    mm_status st;
    mm_transfer t;
    char err[128];
    char path[256];
    struct sockaddr_un addr;
    server srv;
    pthread_t thread;
    snprintf(path, sizeof(path), "%s/daemon.sock", dir);
    srv.stop = 0;
    srv.listener = socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, strlen(path) + 1);
    if (bind(srv.listener, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(srv.listener, 4) != 0) {
        perror("listen");
        failures++;
        return;
    }
    pthread_create(&thread, NULL, serve, &srv);
    setenv("VOXKIT_SOCKET", path, 1);
    mm_init(&be);

    check(mm_list(&be, &snap, err, sizeof(err)) == 0, "online: list");
    check_snapshot(&snap, 1, "online");

    memset(&st, 0, sizeof(st));
    check(mm_install(&be, "hy-mt1.5-1.8b", 0, &t, &st) != 0 &&
              strcmp(st.code, "license-required") == 0,
          "online: restricted model without consent -> license-required");
    check(mm_install(&be, "huge", 0, &t, &st) != 0 &&
              strcmp(st.code, "no-space") == 0,
          "online: no-space");
    check(mm_install(&be, "hy-mt1.5-1.8b", 1, &t, &st) == 0 && st.ok &&
              strcmp(t.state, "downloading") == 0 && t.received == 10 &&
              !mm_job_running(&be),
          "online: install with consent");
    check(strcmp(accepted_id, "yes") == 0, "online: accept_license was sent");
    check(mm_progress(&be, &t) && t.received == 55 && t.bytes == 100,
          "online: progress");
    check(mm_cancel(&be, &t) == 0 && strcmp(t.state, "cancelled") == 0,
          "online: cancel");
    check(mm_remove(&be, "qwen3-asr-0.6b", &st) != 0 &&
              strcmp(st.code, "in-use") == 0,
          "online: deleting a model in use is refused");
    check(mm_remove(&be, "silero-vad", &st) == 0 && st.ok, "online: remove");
    check(mm_select(&be, "hy-mt1.5-1.8b", &st) == 0, "online: select");
    check(mm_verify(&be, "qwen3-asr-0.6b", &t, &st) == 0 &&
              strcmp(t.state, "verifying") == 0,
          "online: verify starts a re-hash");

    srv.stop = 1;
    shutdown(srv.listener, SHUT_RDWR);
    close(srv.listener);
    mm_close(&be);
    pthread_join(thread, NULL);
}

static void test_text(void) {
    char out[256];
    mm_format_bytes(out, sizeof(out), 1133080512ULL);
    check(strcmp(out, "1.1 GB") == 0, "bytes: GB");
    mm_format_bytes(out, sizeof(out), 2500000ULL);
    check(strcmp(out, "2.5 MB") == 0, "bytes: MB");
    mm_format_bytes(out, sizeof(out), 0);
    check(strcmp(out, "0 B") == 0, "bytes: zero");
    mm_error_text("license-required", "x", out, sizeof(out));
    check(strstr(out, "授權") != NULL, "text: license-required");
    mm_error_text("in-use", "x", out, sizeof(out));
    check(strstr(out, "使用中") != NULL, "text: in-use");
    mm_error_text("no-space", "need 2 GB", out, sizeof(out));
    check(strstr(out, "need 2 GB") != NULL && strstr(out, "磁碟") != NULL,
          "text: no-space carries the daemon's numbers");
    mm_error_text("weird", "something", out, sizeof(out));
    check(strcmp(out, "something") == 0,
          "text: unknown code shows the message");
    check(mm_verify_text("ok", "M", out, sizeof(out)) == 0 &&
              strstr(out, "通過") != NULL,
          "text: verify ok");
    check(mm_verify_text("corrupt", "M", out, sizeof(out)) == 1 &&
              strstr(out, "損毀") != NULL,
          "text: verify corrupt");
    mm_usage_text(0, 0, 0, out, sizeof(out));
    check(strstr(out, "已使用 0 B") != NULL && strstr(out, "未知") != NULL &&
              strstr(out, "可用空間 0 B") == NULL,
          "text: missing free space is 未知, not 0 B");
    mm_usage_text(0, 0, 1, out, sizeof(out));
    check(strstr(out, "可用空間 0 B") != NULL, "text: a reported zero is 0 B");
    mm_usage_text(1500, 9000000000ULL, 1, out, sizeof(out));
    check(strstr(out, "已使用 1.5 KB") != NULL &&
              strstr(out, "可用空間 9.0 GB") != NULL,
          "text: used and free space when reported");
}

static char *save_env(const char *key) {
    const char *value = getenv(key);
    return value ? strdup(value) : NULL;
}

static void restore_env(const char *key, char *saved) {
    if (saved) {
        setenv(key, saved, 1);
        free(saved);
    } else {
        unsetenv(key);
    }
}

static void test_daemon_env(const char *dir) {
    char unit[256];
    char *model;
    char *store;
    char *mt;
    char *home;
    FILE *f;
    const char *path;

    model = save_env("VOXKIT_MODEL");
    store = save_env("VOXKIT_MODELS_DIR");
    mt = save_env("VOXKIT_MT_MODEL");
    home = save_env("HOME");

    unsetenv("VOXKIT_MODEL");
    unsetenv("VOXKIT_MODELS_DIR");
    unsetenv("VOXKIT_MT_MODEL");
    setenv("HOME", "/tmp/mm-home", 1);
    mm_apply_daemon_env("/no/such/voxkit.service");
    check(strcmp(getenv("VOXKIT_MODEL") ? getenv("VOXKIT_MODEL") : "",
                 "qwen3-asr-0.6b") == 0,
          "env: default preferred model matches the user unit");
    path = getenv("VOXKIT_MODELS_DIR");
    check(path && strcmp(path, "/tmp/mm-home/.local/share/voxkit/models") == 0,
          "env: default store is ~/.local/share/voxkit/models");

    unsetenv("VOXKIT_MODEL");
    unsetenv("VOXKIT_MODELS_DIR");
    snprintf(unit, sizeof(unit), "%s/voxkit.service", dir);
    f = fopen(unit, "w");
    if (!f) {
        perror("unit");
        failures++;
        goto done;
    }
    fputs("[Service]\n"
          "Environment=VOXKIT_MODEL=whisper-tiny\n"
          "Environment=VOXKIT_MODELS_DIR=%h/.custom/models\n"
          "Environment=VOXKIT_MT_MODEL=hy-mt1.5-1.8b\n",
          f);
    fclose(f);
    mm_apply_daemon_env(unit);
    check(strcmp(getenv("VOXKIT_MODEL"), "whisper-tiny") == 0,
          "env: unit file sets the preferred model");
    check(strcmp(getenv("VOXKIT_MODELS_DIR"), "/tmp/mm-home/.custom/models") ==
              0,
          "env: unit file %h is expanded in the store path");
    check(strcmp(getenv("VOXKIT_MT_MODEL"), "hy-mt1.5-1.8b") == 0,
          "env: unit file sets the translation model");

    setenv("VOXKIT_MODEL", "already-set", 1);
    mm_apply_daemon_env(unit);
    check(strcmp(getenv("VOXKIT_MODEL"), "already-set") == 0,
          "env: an existing value is not overwritten");

done:
    restore_env("VOXKIT_MODEL", model);
    restore_env("VOXKIT_MODELS_DIR", store);
    restore_env("VOXKIT_MT_MODEL", mt);
    restore_env("HOME", home);
}

int main(void) {
    char dir[] = "/tmp/model-backend-XXXXXX";
    char script[256];
    FILE *f;
    if (!mkdtemp(dir)) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(script, sizeof(script), "%s/voxkit", dir);
    f = fopen(script, "w");
    if (!f) {
        perror("fopen");
        return 1;
    }
    fputs(kFakeVoxkit, f);
    fclose(f);
    chmod(script, 0755);
    setenv("VOXKIT_BIN", script, 1);

    test_protocol_fields();
    test_text();
    test_daemon_env(dir);
    test_online(dir);
    test_offline(dir);

    printf("%s\n", failures ? "MODEL BACKEND TESTS FAILED" : "all passed");
    return failures ? 1 : 0;
}
