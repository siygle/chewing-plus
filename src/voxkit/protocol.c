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
