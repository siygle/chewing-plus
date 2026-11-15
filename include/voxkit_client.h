/*
 * SPDX-License-Identifier: MIT
 *
 * Blocking-or-polled client for docs/VOXKIT_PROTOCOL.md. No audio.
 */
#ifndef VOXKIT_CLIENT_H_
#define VOXKIT_CLIENT_H_

#include "voxkit_protocol.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct voxkit_client {
    int fd;
    char in[16384];
    size_t in_len;
} voxkit_client;

void voxkit_client_init(voxkit_client *client);
/* 0 on success. `path` NULL uses voxkit_proto_default_socket. */
int voxkit_client_connect(voxkit_client *client, const char *path);
void voxkit_client_close(voxkit_client *client);
int voxkit_client_fd(const voxkit_client *client);

/* `line` includes the trailing newline. 0 on success. */
int voxkit_client_send(voxkit_client *client, const char *line);

/* 1 when `msg` is filled, 0 on timeout, -1 on disconnect or a bad line
 * (`err` then explains it). `timeout_ms` 0 polls, <0 waits. */
int voxkit_client_next(voxkit_client *client, voxkit_message *msg,
                       int timeout_ms, char *err, size_t err_n);

#ifdef __cplusplus
}
#endif

#endif /* VOXKIT_CLIENT_H_ */
