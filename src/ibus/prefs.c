/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "prefs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int has_token(const char *list, const char *token) {
    return list && strstr(list, token) != NULL;
}

static int config_path(char *buf, size_t n) {
    const char *env = getenv("CHEWING_PLUS_IBUS_CONFIG");
    const char *xdg;
    const char *home;
    int wrote;
    if (env && env[0]) {
        if (strlen(env) + 1 > n) {
            return -1;
        }
        memcpy(buf, env, strlen(env) + 1);
        return 0;
    }
    xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        wrote = snprintf(buf, n, "%s/chewing-plus/ibus.conf", xdg);
        return wrote > 0 && (size_t)wrote < n ? 0 : -1;
    }
    home = getenv("HOME");
    if (!home || !home[0]) {
        return -1;
    }
    wrote = snprintf(buf, n, "%s/.config/chewing-plus/ibus.conf", home);
    return wrote > 0 && (size_t)wrote < n ? 0 : -1;
}

static void from_list(chewing_plus_ibus_prefs *out, const char *list) {
    out->emoji = has_token(list, "emoji");
    out->calculator = has_token(list, "calculator");
    out->space = has_token(list, "space");
    out->shortcode = has_token(list, "shortcode");
    out->english = has_token(list, "english");
    out->suggestions = has_token(list, "suggest");
    out->hint_commit = has_token(list, "hintcommit");
    out->easy_symbol = has_token(list, "easysymbol");
    out->shift_toggle = has_token(list, "shift");
}

static void read_file(chewing_plus_ibus_prefs *out, const char *path) {
    FILE *file = fopen(path, "r");
    char line[128];
    if (!file) {
        return;
    }
    while (fgets(line, sizeof(line), file)) {
        char key[32];
        int value = 0;
        if (sscanf(line, "%31[^=]=%d", key, &value) != 2) {
            continue;
        }
        value = value ? 1 : 0;
        if (strcmp(key, "emoji") == 0) {
            out->emoji = value;
        } else if (strcmp(key, "calculator") == 0) {
            out->calculator = value;
        } else if (strcmp(key, "space") == 0) {
            out->space = value;
        } else if (strcmp(key, "shortcode") == 0) {
            out->shortcode = value;
        } else if (strcmp(key, "english") == 0) {
            out->english = value;
        } else if (strcmp(key, "suggestions") == 0) {
            out->suggestions = value;
        } else if (strcmp(key, "hint_commit") == 0) {
            out->hint_commit = value;
        } else if (strcmp(key, "easy_symbol") == 0) {
            out->easy_symbol = value;
        } else if (strcmp(key, "shift_toggle") == 0) {
            out->shift_toggle = value;
        }
    }
    fclose(file);
}

void chewing_plus_ibus_prefs_load(chewing_plus_ibus_prefs *out) {
    const char *features;
    char path[512];
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    features = getenv("CHEWING_PLUS_IBUS_FEATURES");
    if (features) {
        from_list(out, features);
        return;
    }
    if (config_path(path, sizeof(path)) == 0) {
        read_file(out, path);
    }
}

static int mkdir_parent(const char *file) {
    char dir[512];
    char *slash;
    char *cursor;
    size_t len = strlen(file);
    if (len + 1 > sizeof(dir)) {
        return -1;
    }
    memcpy(dir, file, len + 1);
    slash = strrchr(dir, '/');
    if (!slash) {
        return 0;
    }
    *slash = '\0';
    cursor = dir[0] == '/' ? dir + 1 : dir;
    for (; *cursor; cursor++) {
        if (*cursor != '/') {
            continue;
        }
        *cursor = '\0';
        if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
            return -1;
        }
        *cursor = '/';
    }
    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

int chewing_plus_ibus_prefs_save(const chewing_plus_ibus_prefs *in) {
    char path[512];
    FILE *file;
    if (!in || config_path(path, sizeof(path)) != 0) {
        return -1;
    }
    if (mkdir_parent(path) != 0) {
        return -1;
    }
    file = fopen(path, "w");
    if (!file) {
        return -1;
    }
    fprintf(file, "emoji=%d\n", in->emoji ? 1 : 0);
    fprintf(file, "calculator=%d\n", in->calculator ? 1 : 0);
    fprintf(file, "space=%d\n", in->space ? 1 : 0);
    fprintf(file, "shortcode=%d\n", in->shortcode ? 1 : 0);
    fprintf(file, "english=%d\n", in->english ? 1 : 0);
    fprintf(file, "suggestions=%d\n", in->suggestions ? 1 : 0);
    fprintf(file, "hint_commit=%d\n", in->hint_commit ? 1 : 0);
    fprintf(file, "easy_symbol=%d\n", in->easy_symbol ? 1 : 0);
    fprintf(file, "shift_toggle=%d\n", in->shift_toggle ? 1 : 0);
    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}
