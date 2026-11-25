/*
 * SPDX-License-Identifier: MIT
 */
#include "voxkit_client.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void voxkit_client_init(voxkit_client *client) {
    if (!client) {
        return;
    }
    client->fd = -1;
    client->in_len = 0;
    client->last_line[0] = '\0';
}

int voxkit_client_fd(const voxkit_client *client) {
    return client ? client->fd : -1;
}

int voxkit_client_connect(voxkit_client *client, const char *path) {
    char fallback[256];
    struct sockaddr_un addr;
    const char *use = path;
    if (!client) {
        return -1;
    }
    voxkit_client_close(client);
    if (!use || !use[0]) {
        if (voxkit_proto_default_socket(fallback, sizeof(fallback)) !=
            VOXKIT_PROTO_OK) {
            return -1;
        }
        use = fallback;
    }
    if (strlen(use) >= sizeof(addr.sun_path)) {
        return -1;
    }
    client->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client->fd < 0) {
        return -1;
    }
    fcntl(client->fd, F_SETFD, FD_CLOEXEC);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, use, strlen(use) + 1);
    if (connect(client->fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        voxkit_client_close(client);
        return -1;
    }
    return 0;
}

void voxkit_client_close(voxkit_client *client) {
    if (!client) {
        return;
    }
    if (client->fd >= 0) {
        close(client->fd);
        client->fd = -1;
    }
    client->in_len = 0;
}

int voxkit_client_send(voxkit_client *client, const char *line) {
    size_t left;
    const char *p;
    if (!client || client->fd < 0 || !line) {
        return -1;
    }
    left = strlen(line);
    p = line;
    while (left) {
        ssize_t n = send(client->fd, p, left, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        p += n;
        left -= (size_t)n;
    }
    return 0;
}

static int take_line(voxkit_client *client, voxkit_message *msg, char *err,
                     size_t err_n) {
    char *nl = memchr(client->in, '\n', client->in_len);
    char line[8192];
    size_t len;
    int rc;
    if (!nl) {
        return 0;
    }
    len = (size_t)(nl - client->in);
    if (len >= sizeof(line)) {
        voxkit_client_close(client);
        return -1;
    }
    memcpy(line, client->in, len);
    line[len] = '\0';
    memcpy(client->last_line, line, len + 1);
    client->in_len -= len + 1;
    memmove(client->in, nl + 1, client->in_len);
    rc = voxkit_proto_parse(line, msg, err, err_n);
    return rc == VOXKIT_PROTO_OK ? 1 : -1;
}

int voxkit_client_next(voxkit_client *client, voxkit_message *msg,
                       int timeout_ms, char *err, size_t err_n) {
    int ready;
    if (!client || client->fd < 0 || !msg) {
        return -1;
    }
    ready = take_line(client, msg, err, err_n);
    if (ready != 0) {
        return ready;
    }
    while (1) {
        struct pollfd pfd;
        char buf[1024];
        ssize_t n;
        pfd.fd = client->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        n = poll(&pfd, 1, timeout_ms);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        n = recv(client->fd, buf, sizeof(buf), 0);
        if (n == 0) {
            voxkit_client_close(client);
            return -1;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (client->in_len + (size_t)n > sizeof(client->in)) {
            voxkit_client_close(client);
            return -1;
        }
        memcpy(client->in + client->in_len, buf, (size_t)n);
        client->in_len += (size_t)n;
        ready = take_line(client, msg, err, err_n);
        if (ready != 0) {
            return ready;
        }
        timeout_ms = 0;
    }
}

static void set_client_err(char *err, size_t n, const char *text) {
    if (err && n) {
        snprintf(err, n, "%s", text);
    }
}

static int roundtrip(voxkit_client *client, const char *line,
                     voxkit_msg_type expect, voxkit_message *reply,
                     int timeout_ms, char *err, size_t err_n) {
    voxkit_message local;
    voxkit_message *dest = reply ? reply : &local;
    int rc;
    if (voxkit_client_send(client, line) != 0) {
        set_client_err(err, err_n, "send failed");
        return -1;
    }
    rc = voxkit_client_next(client, dest, timeout_ms, err, err_n);
    if (rc != 1) {
        if (rc == 0) {
            set_client_err(err, err_n, "timeout");
        }
        return -1;
    }
    if (dest->type == VOXKIT_MSG_ERROR) {
        set_client_err(err, err_n,
                       dest->message[0] ? dest->message : dest->code);
        return -1;
    }
    if (dest->type != expect) {
        set_client_err(err, err_n, "unexpected reply");
        return -1;
    }
    return 0;
}

int voxkit_client_model_list(voxkit_client *client, uint64_t req,
                             voxkit_model_info *models, size_t cap,
                             size_t *count, voxkit_message *summary,
                             int timeout_ms, char *err, size_t err_n) {
    char line[128];
    if (voxkit_proto_write_model_list(line, sizeof(line), req) < 0) {
        return -1;
    }
    if (roundtrip(client, line, VOXKIT_MSG_MODEL_LIST, summary, timeout_ms, err,
                  err_n) != 0) {
        return -1;
    }
    return voxkit_proto_parse_models(client->last_line, models, cap, count, err,
                                     err_n);
}

int voxkit_client_model_install(voxkit_client *client, uint64_t req,
                                const char *model, voxkit_message *reply,
                                int timeout_ms, char *err, size_t err_n) {
    char line[256];
    if (voxkit_proto_write_model_install(line, sizeof(line), req, model) < 0) {
        return -1;
    }
    return roundtrip(client, line, VOXKIT_MSG_MODEL_PROGRESS, reply, timeout_ms,
                     err, err_n);
}

int voxkit_client_model_progress(voxkit_client *client, uint64_t req,
                                 voxkit_message *reply, int timeout_ms,
                                 char *err, size_t err_n) {
    char line[128];
    if (voxkit_proto_write_model_progress(line, sizeof(line), req) < 0) {
        return -1;
    }
    return roundtrip(client, line, VOXKIT_MSG_MODEL_PROGRESS, reply, timeout_ms,
                     err, err_n);
}

int voxkit_client_model_cancel(voxkit_client *client, uint64_t req,
                               voxkit_message *reply, int timeout_ms, char *err,
                               size_t err_n) {
    char line[128];
    if (voxkit_proto_write_model_cancel(line, sizeof(line), req) < 0) {
        return -1;
    }
    return roundtrip(client, line, VOXKIT_MSG_MODEL_PROGRESS, reply, timeout_ms,
                     err, err_n);
}

int voxkit_client_model_remove(voxkit_client *client, uint64_t req,
                               const char *model, voxkit_message *reply,
                               int timeout_ms, char *err, size_t err_n) {
    char line[256];
    if (voxkit_proto_write_model_remove(line, sizeof(line), req, model) < 0) {
        return -1;
    }
    return roundtrip(client, line, VOXKIT_MSG_MODEL_REMOVE, reply, timeout_ms,
                     err, err_n);
}

int voxkit_client_model_select(voxkit_client *client, uint64_t req,
                               const char *model, voxkit_message *reply,
                               int timeout_ms, char *err, size_t err_n) {
    char line[256];
    if (voxkit_proto_write_model_select(line, sizeof(line), req, model) < 0) {
        return -1;
    }
    return roundtrip(client, line, VOXKIT_MSG_MODEL_SELECT, reply, timeout_ms,
                     err, err_n);
}

int voxkit_client_model_status(voxkit_client *client, uint64_t req,
                               voxkit_message *reply, int timeout_ms, char *err,
                               size_t err_n) {
    char line[128];
    if (voxkit_proto_write_model_status(line, sizeof(line), req) < 0) {
        return -1;
    }
    return roundtrip(client, line, VOXKIT_MSG_MODEL_STATUS, reply, timeout_ms,
                     err, err_n);
}

int voxkit_client_supports_translate(voxkit_client *client, int timeout_ms) {
    voxkit_message reply;
    char err[64];
    if (voxkit_client_model_status(client, 0, &reply, timeout_ms, err,
                                   sizeof(err)) != 0) {
        return 0;
    }
    return reply.caps_translate ? 1 : 0;
}
