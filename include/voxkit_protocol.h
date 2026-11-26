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
    VOXKIT_MSG_PONG,
    /* v1 extension. Old daemons answer these with error/bad-request.
     * Old clients never send them. `v` stays 1. */
    VOXKIT_MSG_MODEL_LIST,
    VOXKIT_MSG_MODEL_INSTALL,
    VOXKIT_MSG_MODEL_PROGRESS,
    VOXKIT_MSG_MODEL_CANCEL,
    VOXKIT_MSG_MODEL_REMOVE,
    VOXKIT_MSG_MODEL_SELECT,
    VOXKIT_MSG_MODEL_STATUS,
    VOXKIT_MSG_MODEL_VERIFY /* "model-manager" capability */
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
    /* Model-management extension. Empty or zero when the line omitted them. */
    char store[512];
    char state[32];
    char daemon[32];
    uint64_t bytes;
    uint64_t received;
    uint64_t store_bytes;
    int installed;
    int active;
    /* Translation extension (fcitx5-voxkit feat/voice-translate). */
    char mode[16];            /* final: "translate" for a translate request */
    char target[16];          /* final: canonical target language */
    char translate_error[64]; /* final: why it fell back to the original */
    char source[4096];        /* final: the transcript before translation */
    int translated;           /* final: 1 when `text` is a translation */
    int caps_translate;       /* model.status/list: "translate" capability */
    /* Model-manager extension ("model-manager" capability). */
    int caps_model_manager;
    uint64_t free_bytes; /* model.list/status: free space in the store */
    int has_free_bytes;  /* 1 when the line carried free_bytes */
    char model_mt[64];   /* model.list/status: selected translation model */
    char result[16];     /* model.verify: ok | missing | corrupt */
} voxkit_message;

/* One catalog row from a model.list line. */
typedef struct voxkit_model_info {
    char id[64];
    char kind[16];
    char blurb[512];
    uint64_t bytes;
    uint64_t size_on_disk;
    int installed;
    int active;
    /* Model-manager extension; empty or zero from an older daemon. */
    char title[128];
    char purpose[8]; /* asr | mt | vad */
    char license[128];
    char restriction[512];
    uint64_t partial_bytes;
    uint64_t runtime_bytes;
    int restricted; /* needs license consent before download */
    int license_accepted;
    int recommended;
    int selectable;
    int downloading;
    /* "store" for a model this user can delete, otherwise where a shared
     * read-only copy was found. Empty from an older daemon. */
    char source[16];
    int removable;
    /* 1 when the line carried the key; an older daemon omits them. */
    int has_removable;
    int has_selectable;
} voxkit_model_info;

/* Room for a whole catalog in one model.list call. */
#define VOXKIT_MODELS_MAX 32

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
/* `start` with `mode` ("dictate" or "translate"; NULL omits it, which
 * every daemon treats as dictate) and `target` (NULL omits it). Send
 * "translate" only to a daemon whose model.status listed the "translate"
 * capability (caps_translate): an older daemon ignores both fields. */
int voxkit_proto_write_start_mode(char *buf, size_t n, uint64_t req,
                                  const char *language, const char *convert,
                                  const char *model,
                                  const char *const *vocabulary,
                                  size_t vocabulary_count, const char *mode,
                                  const char *target);
int voxkit_proto_write_model_list(char *buf, size_t n, uint64_t req);
int voxkit_proto_write_model_progress(char *buf, size_t n, uint64_t req);
int voxkit_proto_write_model_cancel(char *buf, size_t n, uint64_t req);
int voxkit_proto_write_model_status(char *buf, size_t n, uint64_t req);
int voxkit_proto_write_model_install(char *buf, size_t n, uint64_t req,
                                     const char *model);
int voxkit_proto_write_model_remove(char *buf, size_t n, uint64_t req,
                                    const char *model);
int voxkit_proto_write_model_select(char *buf, size_t n, uint64_t req,
                                    const char *model);
/* model.install with accept_license:true (the user ticked the checkbox). */
int voxkit_proto_write_model_install_accept(char *buf, size_t n, uint64_t req,
                                            const char *model);
int voxkit_proto_write_model_verify(char *buf, size_t n, uint64_t req,
                                    const char *model);

/* Fill `out[0..*count)` from a model.list line. `count` may be NULL.
 * Returns VOXKIT_PROTO_ERR_RANGE when more rows exist than `cap` (the
 * first `cap` rows are still written). */
int voxkit_proto_parse_models(const char *line, voxkit_model_info *out,
                              size_t cap, size_t *count, char *err,
                              size_t err_n);

/* VOXKIT_SOCKET, else $XDG_RUNTIME_DIR/voxkit.sock, else
 * /tmp/voxkit-<uid>.sock. Returns 0 or VOXKIT_PROTO_ERR_RANGE. */
int voxkit_proto_default_socket(char *buf, size_t n);

const char *voxkit_proto_type_name(voxkit_msg_type type);

#ifdef __cplusplus
}
#endif

#endif /* VOXKIT_PROTOCOL_H_ */
