/*
 * SPDX-License-Identifier: MIT
 *
 * Toolkit-free model manager. Frontends that cannot draw a download
 * progress bar launch this. It only speaks the voxkit socket protocol.
 *
 *   chewing-plus-models status
 *   chewing-plus-models list
 *   chewing-plus-models install ID [--accept-license]
 *   chewing-plus-models verify ID
 *   chewing-plus-models progress
 *   chewing-plus-models cancel
 *   chewing-plus-models remove ID
 *   chewing-plus-models select ID
 *   chewing-plus-models menu
 */
#define _POSIX_C_SOURCE 200809L

#include "voxkit_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int fail(const char *err) {
    fprintf(stderr, "chewing-plus-models: %s\n", err[0] ? err : "failed");
    return 1;
}

static int connect_client(voxkit_client *client, char *err, size_t err_n) {
    voxkit_client_init(client);
    if (voxkit_client_connect(client, NULL) != 0) {
        snprintf(err, err_n, "voxkit is not running");
        return -1;
    }
    return 0;
}

static void print_status(const voxkit_message *msg) {
    printf("daemon: %s\n", msg->daemon[0] ? msg->daemon : "running");
    printf("download: %s\n", msg->state[0] ? msg->state : "idle");
    printf("active: %s\n", msg->model);
    printf("store: %s\n", msg->store);
    printf("store_bytes: %llu\n", (unsigned long long)msg->store_bytes);
    if (msg->bytes) {
        printf("progress: %llu / %llu\n", (unsigned long long)msg->received,
               (unsigned long long)msg->bytes);
    }
    if (msg->message[0]) {
        printf("message: %s\n", msg->message);
    }
}

static int cmd_status(voxkit_client *client) {
    voxkit_message msg;
    char err[256] = "";
    if (voxkit_client_model_status(client, 1, &msg, 1000, err, sizeof(err)) !=
        0) {
        return fail(err);
    }
    print_status(&msg);
    return 0;
}

static int cmd_list(voxkit_client *client) {
    voxkit_model_info rows[VOXKIT_MODELS_MAX];
    voxkit_message msg;
    size_t count = 0;
    size_t i;
    char err[256] = "";
    if (voxkit_client_model_list(client, 1, rows, VOXKIT_MODELS_MAX, &count,
                                 &msg, 1000, err, sizeof(err)) != 0) {
        return fail(err);
    }
    print_status(&msg);
    for (i = 0; i < count; i++) {
        printf("%s  %s  %s  %llu bytes  on_disk %llu%s%s\n", rows[i].id,
               rows[i].kind, rows[i].installed ? "installed" : "not-installed",
               (unsigned long long)rows[i].bytes,
               (unsigned long long)rows[i].size_on_disk,
               rows[i].active ? "  active" : "",
               rows[i].restricted && !rows[i].license_accepted
                   ? "  license-required"
                   : "");
        if (rows[i].restricted && !rows[i].license_accepted) {
            printf("    %s\n", rows[i].restriction);
        }
    }
    return 0;
}

static int cmd_progress(voxkit_client *client) {
    voxkit_message msg;
    char err[256] = "";
    if (voxkit_client_model_progress(client, 1, &msg, 1000, err, sizeof(err)) !=
        0) {
        return fail(err);
    }
    print_status(&msg);
    return strcmp(msg.state, "error") == 0 ? 1 : 0;
}

static int cmd_install(voxkit_client *client, const char *id, int accept) {
    voxkit_message msg;
    char err[256] = "";
    int spins = 0;
    int rc;
    memset(&msg, 0, sizeof(msg));
    rc = accept ? voxkit_client_model_install_accept(client, 1, id, &msg, 1000,
                                                     err, sizeof(err))
                : voxkit_client_model_install(client, 1, id, &msg, 1000, err,
                                              sizeof(err));
    if (rc != 0) {
        if (!accept && strcmp(msg.code, "license-required") == 0) {
            fprintf(stderr,
                    "chewing-plus-models: %s\n"
                    "re-run with --accept-license to accept it\n",
                    err);
            return 1;
        }
        return fail(err);
    }
    while (spins < 24 * 60 * 60) {
        if (voxkit_client_model_progress(client, 2, &msg, 1000, err,
                                         sizeof(err)) != 0) {
            return fail(err);
        }
        printf("\r%s %s %llu / %llu   ", msg.model, msg.state,
               (unsigned long long)msg.received, (unsigned long long)msg.bytes);
        fflush(stdout);
        if (strcmp(msg.state, "downloading") != 0 &&
            strcmp(msg.state, "verifying") != 0) {
            printf("\n");
            if (msg.message[0]) {
                printf("%s\n", msg.message);
            }
            return strcmp(msg.state, "done") == 0 ? 0 : 1;
        }
        {
            struct timespec pause;
            pause.tv_sec = 0;
            pause.tv_nsec = 200000000L;
            nanosleep(&pause, NULL);
        }
        spins++;
    }
    return fail("timed out");
}

static int cmd_cancel(voxkit_client *client) {
    voxkit_message msg;
    char err[256] = "";
    if (voxkit_client_model_cancel(client, 1, &msg, 1000, err, sizeof(err)) !=
        0) {
        return fail(err);
    }
    print_status(&msg);
    return 0;
}

static int cmd_verify(voxkit_client *client, const char *id) {
    voxkit_message msg;
    char err[256] = "";
    int spins = 0;
    if (voxkit_client_model_verify(client, 1, id, &msg, 1000, err,
                                   sizeof(err)) != 0) {
        return fail(err);
    }
    while (spins++ < 60 * 60 * 5) {
        if (voxkit_client_model_progress(client, 2, &msg, 1000, err,
                                         sizeof(err)) != 0) {
            return fail(err);
        }
        if (strcmp(msg.state, "verifying") != 0) {
            printf("%s %s\n", id, msg.message[0] ? msg.message : msg.state);
            return strcmp(msg.message, "ok") == 0 ? 0 : 1;
        }
        {
            struct timespec pause;
            pause.tv_sec = 0;
            pause.tv_nsec = 200000000L;
            nanosleep(&pause, NULL);
        }
    }
    return fail("timed out");
}

static int cmd_remove(voxkit_client *client, const char *id) {
    voxkit_message msg;
    char err[256] = "";
    if (voxkit_client_model_remove(client, 1, id, &msg, 1000, err,
                                   sizeof(err)) != 0) {
        return fail(err);
    }
    printf("removed %s\n", msg.model);
    return 0;
}

static int cmd_select(voxkit_client *client, const char *id) {
    voxkit_message msg;
    char err[256] = "";
    if (voxkit_client_model_select(client, 1, id, &msg, 1000, err,
                                   sizeof(err)) != 0) {
        return fail(err);
    }
    printf("active %s\n", msg.model);
    return 0;
}

static void usage(void) {
    fprintf(stderr, "usage: chewing-plus-models status|list|progress|cancel\n"
                    "       chewing-plus-models install ID [--accept-license]\n"
                    "       chewing-plus-models verify|remove|select ID\n"
                    "       chewing-plus-models menu\n");
}

static int dispatch(voxkit_client *client, int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        return cmd_status(client);
    }
    if (strcmp(argv[1], "list") == 0) {
        return cmd_list(client);
    }
    if (strcmp(argv[1], "progress") == 0) {
        return cmd_progress(client);
    }
    if (strcmp(argv[1], "cancel") == 0) {
        return cmd_cancel(client);
    }
    if (strcmp(argv[1], "install") == 0 && argc >= 3) {
        return cmd_install(client, argv[2],
                           argc >= 4 &&
                               strcmp(argv[3], "--accept-license") == 0);
    }
    if (strcmp(argv[1], "verify") == 0 && argc >= 3) {
        return cmd_verify(client, argv[2]);
    }
    if (strcmp(argv[1], "remove") == 0 && argc >= 3) {
        return cmd_remove(client, argv[2]);
    }
    if (strcmp(argv[1], "select") == 0 && argc >= 3) {
        return cmd_select(client, argv[2]);
    }
    usage();
    return 2;
}

static int menu(voxkit_client *client) {
    char line[128];
    printf("1 status  2 list  3 install  4 cancel  5 remove  6 select  q "
           "quit\n");
    while (fgets(line, sizeof(line), stdin)) {
        char id[64];
        int choice = line[0];
        if (choice == 'q' || choice == 'Q') {
            return 0;
        }
        if (choice == '1') {
            cmd_status(client);
        } else if (choice == '2') {
            cmd_list(client);
        } else if (choice == '3' || choice == '5' || choice == '6') {
            printf("model id: ");
            fflush(stdout);
            if (!fgets(id, sizeof(id), stdin)) {
                return 0;
            }
            id[strcspn(id, "\r\n")] = '\0';
            if (choice == '3') {
                cmd_install(client, id, 0);
            } else if (choice == '5') {
                cmd_remove(client, id);
            } else {
                cmd_select(client, id);
            }
        } else if (choice == '4') {
            cmd_cancel(client);
        } else {
            printf("1-6 or q\n");
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    voxkit_client client;
    char err[256] = "";
    int rc;
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage();
        return 0;
    }
    if (connect_client(&client, err, sizeof(err)) != 0) {
        return fail(err);
    }
    if (argc >= 2 && strcmp(argv[1], "menu") == 0) {
        rc = menu(&client);
    } else {
        rc = dispatch(&client, argc, argv);
    }
    voxkit_client_close(&client);
    return rc;
}
