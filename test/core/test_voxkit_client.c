/*
 * SPDX-License-Identifier: MIT
 *
 * Headless push-to-talk: a fake voxkit daemon on a Unix socket, no microphone.
 */
#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L

#include "voxkit_client.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int failures = 0;

static void check(int ok, const char *what) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

static volatile int translate_caps = 0;

static void *serve(void *arg) {
    int fd = *(int *)arg;
    char buf[1024];
    size_t len = 0;
    while (len < sizeof(buf) - 1) {
        ssize_t n = recv(fd, buf + len, sizeof(buf) - 1 - len, 0);
        char *nl;
        if (n <= 0) {
            break;
        }
        len += (size_t)n;
        buf[len] = '\0';
        while ((nl = strchr(buf, '\n')) != NULL) {
            voxkit_message msg;
            char reply[1024];
            *nl = '\0';
            if (voxkit_proto_parse(buf, &msg, NULL, 0) != VOXKIT_PROTO_OK) {
                break;
            }
            reply[0] = '\0';
            if (msg.type == VOXKIT_MSG_PING) {
                snprintf(reply, sizeof(reply),
                         "{\"v\":1,\"type\":\"pong\",\"req\":%llu}\n",
                         (unsigned long long)msg.req);
            } else if (msg.type == VOXKIT_MSG_START) {
                snprintf(reply, sizeof(reply),
                         "{\"v\":1,\"type\":\"ready\",\"req\":%llu}\n"
                         "{\"v\":1,\"type\":\"partial\",\"req\":%llu,"
                         "\"stable\":\"\",\"tentative\":\"聆聽中\"}\n",
                         (unsigned long long)msg.req,
                         (unsigned long long)msg.req);
            } else if (msg.type == VOXKIT_MSG_MODEL_STATUS && msg.req == 0 &&
                       translate_caps) {
                snprintf(reply, sizeof(reply),
                         "{\"v\":1,\"type\":\"model.status\",\"req\":0,"
                         "\"state\":\"idle\",\"capabilities\":[\"translate\"],"
                         "\"translate\":{\"plugin\":true}}\n");
            } else if (msg.type == VOXKIT_MSG_MODEL_STATUS) {
                snprintf(reply, sizeof(reply),
                         "{\"v\":1,\"type\":\"model.status\",\"req\":%llu,"
                         "\"daemon\":\"running\",\"state\":\"idle\","
                         "\"store\":\"/tmp/models\",\"store_bytes\":0,"
                         "\"model\":\"\"}\n",
                         (unsigned long long)msg.req);
            } else if (msg.type == VOXKIT_MSG_MODEL_LIST) {
                snprintf(
                    reply, sizeof(reply),
                    "{\"v\":1,\"type\":\"model.list\",\"req\":%llu,"
                    "\"daemon\":\"running\",\"store\":\"/tmp/models\","
                    "\"store_bytes\":0,\"model\":\"\","
                    "\"models\":[{\"id\":\"whisper-tiny\",\"kind\":\"asr\","
                    "\"bytes\":10,\"installed\":false,\"active\":false,"
                    "\"blurb\":\"tiny\"}]}\n",
                    (unsigned long long)msg.req);
            } else if (msg.type == VOXKIT_MSG_STOP) {
                snprintf(reply, sizeof(reply),
                         "{\"v\":1,\"type\":\"final\",\"req\":%llu,"
                         "\"text\":\"測試\"}\n",
                         (unsigned long long)msg.req);
            }
            if (reply[0]) {
                send(fd, reply, strlen(reply), MSG_NOSIGNAL);
            }
            memmove(buf, nl + 1, strlen(nl + 1) + 1);
            len = strlen(buf);
            if (msg.type == VOXKIT_MSG_STOP) {
                close(fd);
                return NULL;
            }
        }
    }
    close(fd);
    return NULL;
}

int main(void) {
    char dir[] = "/tmp/voxkit-client-XXXXXX";
    char path[128];
    int listener;
    struct sockaddr_un addr;
    pthread_t thread;
    int accepted;
    voxkit_client client;
    voxkit_message msg;
    char line[256];
    char err[64];
    if (!mkdtemp(dir)) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(path, sizeof(path), "%s/sock", dir);
    listener = socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(path) >= sizeof(addr.sun_path)) {
        return 1;
    }
    memcpy(addr.sun_path, path, strlen(path) + 1);
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(listener, 1) != 0) {
        perror("listen");
        return 1;
    }
    voxkit_client_init(&client);
    check(voxkit_client_connect(&client, path) == 0, "connect fake daemon");
    accepted = accept(listener, NULL, NULL);
    pthread_create(&thread, NULL, serve, &accepted);

    check(voxkit_proto_write_ping(line, sizeof(line), 4) > 0, "ping line");
    check(voxkit_client_send(&client, line) == 0, "send ping");
    check(voxkit_client_next(&client, &msg, 1000, err, sizeof(err)) == 1,
          "read pong");
    check(msg.type == VOXKIT_MSG_PONG && msg.req == 4, "pong req");

    {
        voxkit_model_info rows[2];
        size_t count = 99;
        check(voxkit_client_model_status(&client, 8, &msg, 1000, err,
                                         sizeof(err)) == 0,
              "status round trip");
        check(strcmp(msg.daemon, "running") == 0 &&
                  strcmp(msg.state, "idle") == 0,
              "daemon idle");
        check(voxkit_client_model_list(&client, 9, rows, 2, &count, &msg, 1000,
                                       err, sizeof(err)) == 0,
              "list round trip");
        check(count == 1 && strcmp(rows[0].id, "whisper-tiny") == 0 &&
                  !rows[0].installed,
              "catalog row");
    }

    check(voxkit_client_supports_translate(&client, 1000) == 0,
          "old daemon: no translate capability");
    translate_caps = 1;
    check(voxkit_client_supports_translate(&client, 1000) == 1,
          "new daemon: translate capability");

    check(voxkit_proto_write_start(line, sizeof(line), 1, "zh", "s2twp", NULL,
                                   NULL, 0) > 0,
          "start line");
    check(voxkit_client_send(&client, line) == 0, "send start");
    check(voxkit_client_next(&client, &msg, 1000, err, sizeof(err)) == 1 &&
              msg.type == VOXKIT_MSG_READY,
          "ready");
    check(voxkit_client_next(&client, &msg, 1000, err, sizeof(err)) == 1 &&
              msg.type == VOXKIT_MSG_PARTIAL &&
              strcmp(msg.tentative, "聆聽中") == 0,
          "listening partial");
    check(voxkit_proto_write_stop(line, sizeof(line), 1) > 0, "stop line");
    check(voxkit_client_send(&client, line) == 0, "send stop");
    check(voxkit_client_next(&client, &msg, 1000, err, sizeof(err)) == 1 &&
              msg.type == VOXKIT_MSG_FINAL && strcmp(msg.text, "測試") == 0,
          "final text");

    pthread_join(thread, NULL);
    voxkit_client_close(&client);
    close(listener);
    unlink(path);
    rmdir(dir);
    printf(failures ? "CLIENT TESTS FAILED (%d)\n" : "CLIENT TESTS OK\n",
           failures);
    return failures ? 1 : 0;
}
