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
    char last_line[8192];
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

/* One round trip. `summary` receives the reply line's flat fields.
 * `models` receives catalog rows for model.list (may be NULL). */
int voxkit_client_model_list(voxkit_client *client, uint64_t req,
                             voxkit_model_info *models, size_t cap,
                             size_t *count, voxkit_message *summary,
                             int timeout_ms, char *err, size_t err_n);
int voxkit_client_model_install(voxkit_client *client, uint64_t req,
                                const char *model, voxkit_message *reply,
                                int timeout_ms, char *err, size_t err_n);
int voxkit_client_model_progress(voxkit_client *client, uint64_t req,
                                 voxkit_message *reply, int timeout_ms,
                                 char *err, size_t err_n);
int voxkit_client_model_cancel(voxkit_client *client, uint64_t req,
                               voxkit_message *reply, int timeout_ms, char *err,
                               size_t err_n);
int voxkit_client_model_remove(voxkit_client *client, uint64_t req,
                               const char *model, voxkit_message *reply,
                               int timeout_ms, char *err, size_t err_n);
int voxkit_client_model_select(voxkit_client *client, uint64_t req,
                               const char *model, voxkit_message *reply,
                               int timeout_ms, char *err, size_t err_n);
int voxkit_client_model_status(voxkit_client *client, uint64_t req,
                               voxkit_message *reply, int timeout_ms, char *err,
                               size_t err_n);

/* 1 when the daemon's model.status lists the "translate" capability, 0 for
 * an older daemon, an error or a timeout. Blocks up to `timeout_ms`; call it
 * once per connection, before the first translate start. */
int voxkit_client_supports_translate(voxkit_client *client, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* VOXKIT_CLIENT_H_ */
