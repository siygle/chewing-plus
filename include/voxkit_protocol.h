/*
 * SPDX-License-Identifier: MIT
 *
 * Version 1 of the voxkit Unix-socket protocol. Normative text:
 * docs/VOXKIT_PROTOCOL.md. This matches fcitx5-voxkit main (daemon
 * protocol.rs and module/VoiceClient.cpp). The fcitx5 chewing addon does
 * not speak the socket itself; it keeps using the voxkit module.
 */
#ifndef VOXKIT_PROTOCOL_H_
#define VOXKIT_PROTOCOL_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VOXKIT_PROTO_VERSION 1

enum {
    VOXKIT_PROTO_OK = 0,
    VOXKIT_PROTO_ERR_PARSE = 1,
    VOXKIT_PROTO_ERR_VERSION = 2,
    VOXKIT_PROTO_ERR_TYPE = 3,
    VOXKIT_PROTO_ERR_RANGE = 4
};

typedef enum voxkit_msg_type {
    VOXKIT_MSG_UNKNOWN = 0,
    VOXKIT_MSG_START,
    VOXKIT_MSG_STOP,
    VOXKIT_MSG_CANCEL,
    VOXKIT_MSG_SET_VOCABULARY,
    VOXKIT_MSG_PING,
    VOXKIT_MSG_READY,
    VOXKIT_MSG_PARTIAL,
    VOXKIT_MSG_FINAL,
    VOXKIT_MSG_DISCARDED,
    VOXKIT_MSG_ERROR,
    VOXKIT_MSG_LEVEL,
    VOXKIT_MSG_PONG
} voxkit_msg_type;

/* Flat view of one JSON object. Strings are UTF-8 and NUL-terminated.
 * Unused fields are empty. `version` is 0 when the object omitted `v`
 * (the current daemon accepts that as version 1). */
typedef struct voxkit_message {
    int version;
    voxkit_msg_type type;
    uint64_t req;
    char language[16];
    char convert[16];
    char model[64];
    char text[4096];
    char stable[1024];
    char tentative[1024];
    char code[64];
    char reason[64];
    char message[1024];
    double rms_dbfs;
    int has_rms;
} voxkit_message;

/* Parse one line. A trailing newline is optional. `err` may be NULL. */
int voxkit_proto_parse(const char *line, voxkit_message *out, char *err,
                       size_t err_n);

/* Writers append a newline and a NUL. Return the byte count excluding the
 * NUL, or -1 if the buffer is too small. `language` and `convert` NULL
 * become "zh" and "none". `model` NULL omits the field. */
int voxkit_proto_write_ping(char *buf, size_t n, uint64_t req);
int voxkit_proto_write_stop(char *buf, size_t n, uint64_t req);
int voxkit_proto_write_cancel(char *buf, size_t n, uint64_t req);
int voxkit_proto_write_start(char *buf, size_t n, uint64_t req,
                             const char *language, const char *convert,
                             const char *model, const char *const *vocabulary,
                             size_t vocabulary_count);

/* VOXKIT_SOCKET, else $XDG_RUNTIME_DIR/voxkit.sock, else
 * /tmp/voxkit-<uid>.sock. Returns 0 or VOXKIT_PROTO_ERR_RANGE. */
int voxkit_proto_default_socket(char *buf, size_t n);

const char *voxkit_proto_type_name(voxkit_msg_type type);

#ifdef __cplusplus
}
#endif

#endif /* VOXKIT_PROTOCOL_H_ */
