/*
 * SPDX-License-Identifier: MIT
 */
#include "voxkit_protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void set_err(char *err, size_t n, const char *text) {
    if (err && n) {
        snprintf(err, n, "%s", text);
    }
}

static const char *skip(const char *p) {
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

static int hex4(const char *p, unsigned *out) {
    unsigned v = 0;
    int i;
    for (i = 0; i < 4; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') {
            v |= (unsigned)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            v |= (unsigned)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            v |= (unsigned)(c - 'A' + 10);
        } else {
            return -1;
        }
    }
    *out = v;
    return 0;
}

static void utf8_put(char *dest, size_t cap, size_t *len, unsigned cp) {
    char tmp[4];
    int n = 0;
    if (cp < 0x80) {
        tmp[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        tmp[0] = (char)(0xC0 | (cp >> 6));
        tmp[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp < 0x10000) {
        tmp[0] = (char)(0xE0 | (cp >> 12));
        tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        tmp[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else {
        tmp[0] = (char)(0xF0 | (cp >> 18));
        tmp[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        tmp[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        tmp[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }
    if (*len + (size_t)n >= cap) {
        return;
    }
    memcpy(dest + *len, tmp, (size_t)n);
    *len += (size_t)n;
}

/* Parse a JSON string. *pp points at the opening quote. */
static int parse_string(const char **pp, char *dest, size_t cap) {
    const char *p = *pp;
    size_t len = 0;
    if (*p != '"') {
        return -1;
    }
    p++;
    while (*p && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            char e = *p++;
            switch (e) {
            case '"':
            case '\\':
            case '/':
                c = (unsigned char)e;
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u': {
                unsigned cp = 0;
                if (hex4(p, &cp) != 0) {
                    return -1;
                }
                p += 4;
                utf8_put(dest, cap, &len, cp);
                continue;
            }
            default:
                return -1;
            }
        }
        if (len + 1 >= cap) {
            return -1;
        }
        dest[len++] = (char)c;
    }
    if (*p != '"') {
        return -1;
    }
    dest[len] = '\0';
    *pp = p + 1;
    return 0;
}

static int parse_bool(const char **pp, int *out) {
    const char *p = *pp;
    if (strncmp(p, "true", 4) == 0) {
        *out = 1;
        *pp = p + 4;
        return 0;
    }
    if (strncmp(p, "false", 5) == 0) {
        *out = 0;
        *pp = p + 5;
        return 0;
    }
    return -1;
}

static int parse_number(const char **pp, double *out) {
    char *end = NULL;
    *out = strtod(*pp, &end);
    if (end == *pp) {
        return -1;
    }
    *pp = end;
    return 0;
}

static int skip_value(const char **pp) {
    const char *p = skip(*pp);
    if (*p == '"') {
        char dump[8];
        /* Re-parse into a tiny buffer by walking escapes ourselves. */
        p++;
        while (*p && *p != '"') {
            if (*p == '\\') {
                p++;
                if (*p == 'u') {
                    p += 4;
                }
                if (*p) {
                    p++;
                }
            } else {
                p++;
            }
        }
        if (*p != '"') {
            return -1;
        }
        *pp = p + 1;
        (void)dump;
        return 0;
    }
    if (*p == '{') {
        int depth = 0;
        do {
            if (*p == '{') {
                depth++;
            } else if (*p == '}') {
                depth--;
            } else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && p[1]) {
                        p += 2;
                    } else {
                        p++;
                    }
                }
            }
            if (*p) {
                p++;
            }
        } while (*p && depth > 0);
        *pp = p;
        return depth == 0 ? 0 : -1;
    }
    if (*p == '[') {
        p++;
        p = skip(p);
        if (*p == ']') {
            *pp = p + 1;
            return 0;
        }
        while (*p) {
            if (skip_value(&p) != 0) {
                return -1;
            }
            p = skip(p);
            if (*p == ',') {
                p++;
                p = skip(p);
                continue;
            }
            if (*p == ']') {
                *pp = p + 1;
                return 0;
            }
            return -1;
        }
        return -1;
    }
    if (!strncmp(p, "true", 4) || !strncmp(p, "null", 4)) {
        *pp = p + 4;
        return 0;
    }
    if (!strncmp(p, "false", 5)) {
        *pp = p + 5;
        return 0;
    }
    {
        double ignored = 0;
        if (parse_number(&p, &ignored) != 0) {
            return -1;
        }
        *pp = p;
        return 0;
    }
}

static voxkit_msg_type type_from_name(const char *name) {
    static const struct {
        const char *name;
        voxkit_msg_type type;
    } table[] = {
        {"start", VOXKIT_MSG_START},
        {"stop", VOXKIT_MSG_STOP},
        {"cancel", VOXKIT_MSG_CANCEL},
        {"set_vocabulary", VOXKIT_MSG_SET_VOCABULARY},
        {"ping", VOXKIT_MSG_PING},
        {"ready", VOXKIT_MSG_READY},
        {"partial", VOXKIT_MSG_PARTIAL},
        {"final", VOXKIT_MSG_FINAL},
        {"discarded", VOXKIT_MSG_DISCARDED},
        {"error", VOXKIT_MSG_ERROR},
        {"level", VOXKIT_MSG_LEVEL},
        {"pong", VOXKIT_MSG_PONG},
        {"model.list", VOXKIT_MSG_MODEL_LIST},
        {"model.install", VOXKIT_MSG_MODEL_INSTALL},
        {"model.progress", VOXKIT_MSG_MODEL_PROGRESS},
        {"model.cancel", VOXKIT_MSG_MODEL_CANCEL},
        {"model.remove", VOXKIT_MSG_MODEL_REMOVE},
        {"model.select", VOXKIT_MSG_MODEL_SELECT},
        {"model.status", VOXKIT_MSG_MODEL_STATUS},
        {"model.verify", VOXKIT_MSG_MODEL_VERIFY},
    };
    size_t i;
    for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (strcmp(name, table[i].name) == 0) {
            return table[i].type;
        }
    }
    return VOXKIT_MSG_UNKNOWN;
}

const char *voxkit_proto_type_name(voxkit_msg_type type) {
    switch (type) {
    case VOXKIT_MSG_START:
        return "start";
    case VOXKIT_MSG_STOP:
        return "stop";
    case VOXKIT_MSG_CANCEL:
        return "cancel";
    case VOXKIT_MSG_SET_VOCABULARY:
        return "set_vocabulary";
    case VOXKIT_MSG_PING:
        return "ping";
    case VOXKIT_MSG_READY:
        return "ready";
    case VOXKIT_MSG_PARTIAL:
        return "partial";
    case VOXKIT_MSG_FINAL:
        return "final";
    case VOXKIT_MSG_DISCARDED:
        return "discarded";
    case VOXKIT_MSG_ERROR:
        return "error";
    case VOXKIT_MSG_LEVEL:
        return "level";
    case VOXKIT_MSG_PONG:
        return "pong";
    case VOXKIT_MSG_MODEL_LIST:
        return "model.list";
    case VOXKIT_MSG_MODEL_INSTALL:
        return "model.install";
    case VOXKIT_MSG_MODEL_PROGRESS:
        return "model.progress";
    case VOXKIT_MSG_MODEL_CANCEL:
        return "model.cancel";
    case VOXKIT_MSG_MODEL_REMOVE:
        return "model.remove";
    case VOXKIT_MSG_MODEL_SELECT:
        return "model.select";
    case VOXKIT_MSG_MODEL_STATUS:
        return "model.status";
    case VOXKIT_MSG_MODEL_VERIFY:
        return "model.verify";
    default:
        return "";
    }
}

static void copy_field(char *dest, size_t cap, const char *src) {
    snprintf(dest, cap, "%s", src);
}

int voxkit_proto_parse(const char *line, voxkit_message *out, char *err,
                       size_t err_n) {
    const char *p;
    int saw_type = 0;
    int saw_v = 0;
    if (!line || !out) {
        set_err(err, err_n, "null");
        return VOXKIT_PROTO_ERR_PARSE;
    }
    memset(out, 0, sizeof(*out));
    p = skip(line);
    if (*p != '{') {
        set_err(err, err_n, "expected object");
        return VOXKIT_PROTO_ERR_PARSE;
    }
    p++;
    while (1) {
        char key[64];
        p = skip(p);
        if (*p == '}') {
            break;
        }
        if (parse_string(&p, key, sizeof(key)) != 0) {
            set_err(err, err_n, "bad key");
            return VOXKIT_PROTO_ERR_PARSE;
        }
        p = skip(p);
        if (*p != ':') {
            set_err(err, err_n, "expected colon");
            return VOXKIT_PROTO_ERR_PARSE;
        }
        p = skip(p + 1);
        if (strcmp(key, "v") == 0) {
            double num = 0;
            if (parse_number(&p, &num) != 0) {
                set_err(err, err_n, "bad version");
                return VOXKIT_PROTO_ERR_PARSE;
            }
            out->version = (int)num;
            saw_v = 1;
        } else if (strcmp(key, "type") == 0) {
            char name[32];
            if (parse_string(&p, name, sizeof(name)) != 0) {
                set_err(err, err_n, "bad type");
                return VOXKIT_PROTO_ERR_PARSE;
            }
            out->type = type_from_name(name);
            saw_type = 1;
            if (out->type == VOXKIT_MSG_UNKNOWN) {
                set_err(err, err_n, "unknown type");
                return VOXKIT_PROTO_ERR_TYPE;
            }
        } else if (strcmp(key, "req") == 0) {
            double num = 0;
            if (parse_number(&p, &num) != 0 || num < 0) {
                set_err(err, err_n, "bad req");
                return VOXKIT_PROTO_ERR_PARSE;
            }
            out->req = (uint64_t)num;
        } else if (strcmp(key, "rms_dbfs") == 0) {
            if (parse_number(&p, &out->rms_dbfs) != 0) {
                set_err(err, err_n, "bad level");
                return VOXKIT_PROTO_ERR_PARSE;
            }
            out->has_rms = 1;
        } else if (strcmp(key, "bytes") == 0 || strcmp(key, "received") == 0 ||
                   strcmp(key, "store_bytes") == 0 ||
                   strcmp(key, "free_bytes") == 0) {
            double num = 0;
            if (parse_number(&p, &num) != 0 || num < 0) {
                set_err(err, err_n, "bad size");
                return VOXKIT_PROTO_ERR_PARSE;
            }
            if (strcmp(key, "bytes") == 0) {
                out->bytes = (uint64_t)num;
            } else if (strcmp(key, "received") == 0) {
                out->received = (uint64_t)num;
            } else if (strcmp(key, "free_bytes") == 0) {
                out->free_bytes = (uint64_t)num;
            } else {
                out->store_bytes = (uint64_t)num;
            }
        } else if (strcmp(key, "installed") == 0 ||
                   strcmp(key, "active") == 0 ||
                   strcmp(key, "translated") == 0) {
            int flag = 0;
            if (parse_bool(&p, &flag) != 0) {
                set_err(err, err_n, "bad flag");
                return VOXKIT_PROTO_ERR_PARSE;
            }
            if (strcmp(key, "installed") == 0) {
                out->installed = flag;
            } else if (strcmp(key, "active") == 0) {
                out->active = flag;
            } else {
                out->translated = flag;
            }
        } else if (strcmp(key, "capabilities") == 0 && *p == '[') {
            p = skip(p + 1);
            while (*p != ']') {
                char cap[32];
                if (*p == '"') {
                    if (parse_string(&p, cap, sizeof(cap)) != 0) {
                        set_err(err, err_n, "bad capability");
                        return VOXKIT_PROTO_ERR_PARSE;
                    }
                    if (strcmp(cap, "translate") == 0) {
                        out->caps_translate = 1;
                    } else if (strcmp(cap, "model-manager") == 0) {
                        out->caps_model_manager = 1;
                    }
                } else if (skip_value(&p) != 0) {
                    set_err(err, err_n, "bad capability");
                    return VOXKIT_PROTO_ERR_PARSE;
                }
                p = skip(p);
                if (*p == ',') {
                    p = skip(p + 1);
                } else if (*p != ']') {
                    set_err(err, err_n, "bad capabilities");
                    return VOXKIT_PROTO_ERR_PARSE;
                }
            }
            p++;
        } else if (*p == '"') {
            char value[4096];
            if (parse_string(&p, value, sizeof(value)) != 0) {
                set_err(err, err_n, "bad string");
                return VOXKIT_PROTO_ERR_RANGE;
            }
            if (strcmp(key, "text") == 0) {
                copy_field(out->text, sizeof(out->text), value);
            } else if (strcmp(key, "stable") == 0) {
                copy_field(out->stable, sizeof(out->stable), value);
            } else if (strcmp(key, "tentative") == 0) {
                copy_field(out->tentative, sizeof(out->tentative), value);
            } else if (strcmp(key, "code") == 0) {
                copy_field(out->code, sizeof(out->code), value);
            } else if (strcmp(key, "reason") == 0) {
                copy_field(out->reason, sizeof(out->reason), value);
            } else if (strcmp(key, "message") == 0) {
                copy_field(out->message, sizeof(out->message), value);
            } else if (strcmp(key, "language") == 0) {
                copy_field(out->language, sizeof(out->language), value);
            } else if (strcmp(key, "convert") == 0) {
                copy_field(out->convert, sizeof(out->convert), value);
            } else if (strcmp(key, "model") == 0) {
                copy_field(out->model, sizeof(out->model), value);
            } else if (strcmp(key, "model_mt") == 0) {
                copy_field(out->model_mt, sizeof(out->model_mt), value);
            } else if (strcmp(key, "result") == 0) {
                copy_field(out->result, sizeof(out->result), value);
            } else if (strcmp(key, "store") == 0) {
                copy_field(out->store, sizeof(out->store), value);
            } else if (strcmp(key, "state") == 0) {
                copy_field(out->state, sizeof(out->state), value);
            } else if (strcmp(key, "daemon") == 0) {
                copy_field(out->daemon, sizeof(out->daemon), value);
            } else if (strcmp(key, "mode") == 0) {
                copy_field(out->mode, sizeof(out->mode), value);
            } else if (strcmp(key, "target") == 0) {
                copy_field(out->target, sizeof(out->target), value);
            } else if (strcmp(key, "translate_error") == 0) {
                copy_field(out->translate_error, sizeof(out->translate_error),
                           value);
            } else if (strcmp(key, "source") == 0) {
                copy_field(out->source, sizeof(out->source), value);
            }
        } else if (skip_value(&p) != 0) {
            set_err(err, err_n, "bad value");
            return VOXKIT_PROTO_ERR_PARSE;
        }
        p = skip(p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            break;
        }
        set_err(err, err_n, "expected comma");
        return VOXKIT_PROTO_ERR_PARSE;
    }
    if (!saw_type) {
        set_err(err, err_n, "missing type");
        return VOXKIT_PROTO_ERR_PARSE;
    }
    if (saw_v && out->version > VOXKIT_PROTO_VERSION) {
        set_err(err, err_n, "unsupported version");
        return VOXKIT_PROTO_ERR_VERSION;
    }
    if (!saw_v) {
        out->version = 0;
    }
    return VOXKIT_PROTO_OK;
}

static int append(char *buf, size_t n, size_t *used, const char *text) {
    size_t len = strlen(text);
    if (*used + len + 1 > n) {
        return -1;
    }
    memcpy(buf + *used, text, len);
    *used += len;
    buf[*used] = '\0';
    return 0;
}

static int append_json_string(char *buf, size_t n, size_t *used,
                              const char *text) {
    const unsigned char *p;
    if (append(buf, n, used, "\"") != 0) {
        return -1;
    }
    for (p = (const unsigned char *)(text ? text : ""); *p; p++) {
        char tmp[8];
        const char *piece = tmp;
        if (*p == '"' || *p == '\\') {
            tmp[0] = '\\';
            tmp[1] = (char)*p;
            tmp[2] = '\0';
        } else if (*p == '\n') {
            piece = "\\n";
        } else if (*p < 0x20) {
            snprintf(tmp, sizeof(tmp), "\\u%04x", *p);
        } else {
            tmp[0] = (char)*p;
            tmp[1] = '\0';
        }
        if (append(buf, n, used, piece) != 0) {
            return -1;
        }
    }
    return append(buf, n, used, "\"");
}

static int finish(char *buf, size_t n, size_t used) {
    if (append(buf, n, &used, "\n") != 0) {
        return -1;
    }
    return (int)used;
}

static int write_simple(char *buf, size_t n, uint64_t req, const char *type) {
    char body[128];
    int wrote;
    size_t used = 0;
    if (!buf || n == 0) {
        return -1;
    }
    buf[0] = '\0';
    wrote =
        snprintf(body, sizeof(body), "{\"v\":1,\"type\":\"%s\",\"req\":%llu}",
                 type, (unsigned long long)req);
    if (wrote < 0 || (size_t)wrote >= sizeof(body)) {
        return -1;
    }
    if (append(buf, n, &used, body) != 0) {
        return -1;
    }
    return finish(buf, n, used);
}

int voxkit_proto_write_model_list(char *buf, size_t n, uint64_t req) {
    return write_simple(buf, n, req, "model.list");
}

int voxkit_proto_write_model_progress(char *buf, size_t n, uint64_t req) {
    return write_simple(buf, n, req, "model.progress");
}

int voxkit_proto_write_model_cancel(char *buf, size_t n, uint64_t req) {
    return write_simple(buf, n, req, "model.cancel");
}

int voxkit_proto_write_model_status(char *buf, size_t n, uint64_t req) {
    return write_simple(buf, n, req, "model.status");
}

static int write_model_id(char *buf, size_t n, uint64_t req, const char *type,
                          const char *model) {
    size_t used = 0;
    char prefix[96];
    int wrote;
    if (!buf || n == 0 || !model) {
        return -1;
    }
    buf[0] = '\0';
    wrote = snprintf(prefix, sizeof(prefix),
                     "{\"v\":1,\"type\":\"%s\",\"req\":%llu,\"model\":", type,
                     (unsigned long long)req);
    if (wrote < 0 || (size_t)wrote >= sizeof(prefix)) {
        return -1;
    }
    if (append(buf, n, &used, prefix) != 0 ||
        append_json_string(buf, n, &used, model) != 0 ||
        append(buf, n, &used, "}") != 0) {
        return -1;
    }
    return finish(buf, n, used);
}

int voxkit_proto_write_model_install(char *buf, size_t n, uint64_t req,
                                     const char *model) {
    return write_model_id(buf, n, req, "model.install", model);
}

int voxkit_proto_write_model_install_accept(char *buf, size_t n, uint64_t req,
                                            const char *model) {
    int rc = write_model_id(buf, n, req, "model.install", model);
    size_t used;
    if (rc < 0) {
        return rc;
    }
    /* Reopen the object: "...}\n" becomes "...,"accept_license":true}\n". */
    used = strlen(buf);
    if (used < 2 || buf[used - 1] != '\n' || buf[used - 2] != '}') {
        return -1;
    }
    used -= 2;
    buf[used] = '\0';
    if (append(buf, n, &used, ",\"accept_license\":true}") != 0) {
        return -1;
    }
    return finish(buf, n, used);
}

int voxkit_proto_write_model_verify(char *buf, size_t n, uint64_t req,
                                    const char *model) {
    return write_model_id(buf, n, req, "model.verify", model);
}

int voxkit_proto_write_model_remove(char *buf, size_t n, uint64_t req,
                                    const char *model) {
    return write_model_id(buf, n, req, "model.remove", model);
}

int voxkit_proto_write_model_select(char *buf, size_t n, uint64_t req,
                                    const char *model) {
    return write_model_id(buf, n, req, "model.select", model);
}

int voxkit_proto_write_ping(char *buf, size_t n, uint64_t req) {
    return write_simple(buf, n, req, "ping");
}

int voxkit_proto_write_stop(char *buf, size_t n, uint64_t req) {
    return write_simple(buf, n, req, "stop");
}

int voxkit_proto_write_cancel(char *buf, size_t n, uint64_t req) {
    return write_simple(buf, n, req, "cancel");
}

int voxkit_proto_write_start(char *buf, size_t n, uint64_t req,
                             const char *language, const char *convert,
                             const char *model, const char *const *vocabulary,
                             size_t vocabulary_count) {
    return voxkit_proto_write_start_mode(buf, n, req, language, convert, model,
                                         vocabulary, vocabulary_count, NULL,
                                         NULL);
}

int voxkit_proto_write_start_mode(char *buf, size_t n, uint64_t req,
                                  const char *language, const char *convert,
                                  const char *model,
                                  const char *const *vocabulary,
                                  size_t vocabulary_count, const char *mode,
                                  const char *target) {
    char prefix[160];
    size_t used = 0;
    size_t i;
    int wrote;
    if (!buf || n == 0) {
        return -1;
    }
    buf[0] = '\0';
    wrote = snprintf(prefix, sizeof(prefix),
                     "{\"v\":1,\"type\":\"start\",\"req\":%llu,\"language\":",
                     (unsigned long long)req);
    if (wrote < 0 || (size_t)wrote >= sizeof(prefix)) {
        return -1;
    }
    if (append(buf, n, &used, prefix) != 0 ||
        append_json_string(buf, n, &used, language ? language : "zh") != 0 ||
        append(buf, n, &used, ",\"convert\":") != 0 ||
        append_json_string(buf, n, &used, convert ? convert : "none") != 0) {
        return -1;
    }
    if (model && model[0]) {
        if (append(buf, n, &used, ",\"model\":") != 0 ||
            append_json_string(buf, n, &used, model) != 0) {
            return -1;
        }
    }
    if (mode && mode[0]) {
        if (append(buf, n, &used, ",\"mode\":") != 0 ||
            append_json_string(buf, n, &used, mode) != 0) {
            return -1;
        }
    }
    if (target && target[0]) {
        if (append(buf, n, &used, ",\"target\":") != 0 ||
            append_json_string(buf, n, &used, target) != 0) {
            return -1;
        }
    }
    if (append(buf, n, &used, ",\"vocabulary\":[") != 0) {
        return -1;
    }
    for (i = 0; i < vocabulary_count; i++) {
        if (i && append(buf, n, &used, ",") != 0) {
            return -1;
        }
        if (append_json_string(buf, n, &used,
                               vocabulary ? vocabulary[i] : "") != 0) {
            return -1;
        }
    }
    if (append(buf, n, &used, "]}") != 0) {
        return -1;
    }
    return finish(buf, n, used);
}

int voxkit_proto_default_socket(char *buf, size_t n) {
    const char *env;
    int wrote;
    if (!buf || n == 0) {
        return VOXKIT_PROTO_ERR_RANGE;
    }
    env = getenv("VOXKIT_SOCKET");
    if (env && env[0]) {
        if (strlen(env) + 1 > n) {
            return VOXKIT_PROTO_ERR_RANGE;
        }
        memcpy(buf, env, strlen(env) + 1);
        return VOXKIT_PROTO_OK;
    }
    env = getenv("XDG_RUNTIME_DIR");
    if (env && env[0]) {
        wrote = snprintf(buf, n, "%s/voxkit.sock", env);
        if (wrote < 0 || (size_t)wrote >= n) {
            return VOXKIT_PROTO_ERR_RANGE;
        }
        return VOXKIT_PROTO_OK;
    }
    wrote = snprintf(buf, n, "/tmp/voxkit-%lu.sock", (unsigned long)getuid());
    if (wrote < 0 || (size_t)wrote >= n) {
        return VOXKIT_PROTO_ERR_RANGE;
    }
    return VOXKIT_PROTO_OK;
}

static int parse_one_model(const char **pp, voxkit_model_info *info) {
    const char *p = skip(*pp);
    char key[64];
    memset(info, 0, sizeof(*info));
    if (*p != '{') {
        return -1;
    }
    p++;
    while (1) {
        p = skip(p);
        if (*p == '}') {
            p++;
            break;
        }
        if (parse_string(&p, key, sizeof(key)) != 0) {
            return -1;
        }
        p = skip(p);
        if (*p != ':') {
            return -1;
        }
        p = skip(p + 1);
        char *text = NULL;
        size_t text_n = 0;
        uint64_t *number = NULL;
        int *flag = NULL;
        if (strcmp(key, "id") == 0) {
            text = info->id, text_n = sizeof(info->id);
        } else if (strcmp(key, "kind") == 0) {
            text = info->kind, text_n = sizeof(info->kind);
        } else if (strcmp(key, "blurb") == 0) {
            text = info->blurb, text_n = sizeof(info->blurb);
        } else if (strcmp(key, "title") == 0) {
            text = info->title, text_n = sizeof(info->title);
        } else if (strcmp(key, "purpose") == 0) {
            text = info->purpose, text_n = sizeof(info->purpose);
        } else if (strcmp(key, "license") == 0) {
            text = info->license, text_n = sizeof(info->license);
        } else if (strcmp(key, "restriction") == 0) {
            text = info->restriction, text_n = sizeof(info->restriction);
        } else if (strcmp(key, "bytes") == 0) {
            number = &info->bytes;
        } else if (strcmp(key, "size_on_disk") == 0) {
            number = &info->size_on_disk;
        } else if (strcmp(key, "partial_bytes") == 0) {
            number = &info->partial_bytes;
        } else if (strcmp(key, "runtime_bytes") == 0) {
            number = &info->runtime_bytes;
        } else if (strcmp(key, "installed") == 0) {
            flag = &info->installed;
        } else if (strcmp(key, "active") == 0) {
            flag = &info->active;
        } else if (strcmp(key, "restricted") == 0) {
            flag = &info->restricted;
        } else if (strcmp(key, "license_accepted") == 0) {
            flag = &info->license_accepted;
        } else if (strcmp(key, "recommended") == 0) {
            flag = &info->recommended;
        } else if (strcmp(key, "selectable") == 0) {
            flag = &info->selectable;
        } else if (strcmp(key, "downloading") == 0) {
            flag = &info->downloading;
        }
        if (text) {
            /* Truncate long strings instead of failing the whole list. */
            char big[2048];
            if (parse_string(&p, big, sizeof(big)) != 0) {
                return -1;
            }
            copy_field(text, text_n, big);
        } else if (number) {
            double num = 0;
            if (parse_number(&p, &num) != 0 || num < 0) {
                return -1;
            }
            *number = (uint64_t)num;
        } else if (flag) {
            if (parse_bool(&p, flag) != 0) {
                return -1;
            }        } else if (skip_value(&p) != 0) {
            return -1;
        }
        p = skip(p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            p++;
            break;
        }
        return -1;
    }
    *pp = p;
    return 0;
}

int voxkit_proto_parse_models(const char *line, voxkit_model_info *out,
                              size_t cap, size_t *count, char *err,
                              size_t err_n) {
    const char *p;
    size_t stored = 0;
    int overflow = 0;
    if (count) {
        *count = 0;
    }
    if (!line) {
        set_err(err, err_n, "null");
        return VOXKIT_PROTO_ERR_PARSE;
    }
    p = skip(line);
    if (*p != '{') {
        set_err(err, err_n, "expected object");
        return VOXKIT_PROTO_ERR_PARSE;
    }
    p++;
    while (1) {
        char key[64];
        p = skip(p);
        if (*p == '}') {
            break;
        }
        if (parse_string(&p, key, sizeof(key)) != 0) {
            set_err(err, err_n, "bad key");
            return VOXKIT_PROTO_ERR_PARSE;
        }
        p = skip(p);
        if (*p != ':') {
            set_err(err, err_n, "expected colon");
            return VOXKIT_PROTO_ERR_PARSE;
        }
        p = skip(p + 1);
        if (strcmp(key, "models") == 0 && *p == '[') {
            p++;
            while (1) {
                p = skip(p);
                if (*p == ']') {
                    p++;
                    break;
                }
                if (stored < cap && out) {
                    if (parse_one_model(&p, &out[stored]) != 0) {
                        set_err(err, err_n, "bad model");
                        return VOXKIT_PROTO_ERR_PARSE;
                    }
                    stored++;
                } else {
                    overflow = 1;
                    if (skip_value(&p) != 0) {
                        set_err(err, err_n, "bad model");
                        return VOXKIT_PROTO_ERR_PARSE;
                    }
                }
                p = skip(p);
                if (*p == ',') {
                    p++;
                    continue;
                }
                if (*p == ']') {
                    p++;
                    break;
                }
                set_err(err, err_n, "bad models");
                return VOXKIT_PROTO_ERR_PARSE;
            }
        } else if (skip_value(&p) != 0) {
            set_err(err, err_n, "bad value");
            return VOXKIT_PROTO_ERR_PARSE;
        }
        p = skip(p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            break;
        }
        set_err(err, err_n, "expected comma");
        return VOXKIT_PROTO_ERR_PARSE;
    }
    if (count) {
        *count = stored;
    }
    if (overflow) {
        set_err(err, err_n, "too many models");
        return VOXKIT_PROTO_ERR_RANGE;
    }
    return VOXKIT_PROTO_OK;
}
