/*
 * SPDX-License-Identifier: MIT
 */
#include "voxkit_client.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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
