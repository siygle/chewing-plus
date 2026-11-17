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

const char *chewing_plus_ibus_prefs_get(const chewing_plus_ibus_prefs *prefs,
                                        const char *key) {
    size_t i;
    if (!prefs || !key) {
        return NULL;
    }
    for (i = 0; i < prefs->count; i++) {
        if (strcmp(prefs->items[i].key, key) == 0) {
            return prefs->items[i].value;
        }
    }
    return NULL;
}

int chewing_plus_ibus_prefs_set(chewing_plus_ibus_prefs *prefs, const char *key,
                                const char *value) {
    size_t i;
    chewing_plus_ibus_pref *slot = NULL;
    if (!prefs || !key || !value || !key[0] ||
        strlen(key) >= sizeof(prefs->items[0].key) ||
        strlen(value) >= sizeof(prefs->items[0].value)) {
        return -1;
    }
    for (i = 0; i < prefs->count; i++) {
        if (strcmp(prefs->items[i].key, key) == 0) {
            slot = &prefs->items[i];
            break;
        }
    }
    if (!slot) {
        if (prefs->count >= CHEWING_PLUS_IBUS_MAX_PREFS) {
            return -1;
        }
        slot = &prefs->items[prefs->count++];
        strcpy(slot->key, key);
    }
    strcpy(slot->value, value);
    return 0;
}

/* Keys from before the shared settings table. */
static void set_migrated(chewing_plus_ibus_prefs *out, const char *key,
                         const char *value) {
    if (strcmp(key, "space") == 0) {
        chewing_plus_ibus_prefs_set(out, "space_letters", value);
        chewing_plus_ibus_prefs_set(out, "space_digits", value);
    } else if (strcmp(key, "hint_commit") == 0) {
        chewing_plus_ibus_prefs_set(out, "english_hint_commit", value);
    } else {
        chewing_plus_ibus_prefs_set(out, key, value);
    }
}

/* CHEWING_PLUS_IBUS_FEATURES: comma-separated boolean keys, all on. Older
 * short tokens (space, suggest, hintcommit, easysymbol, shift) still work. */
static void from_list(chewing_plus_ibus_prefs *out, const char *list) {
    char copy[512];
    char *token;
    char *save = NULL;
    snprintf(copy, sizeof(copy), "%s", list);
    for (token = strtok_r(copy, ", ", &save); token;
         token = strtok_r(NULL, ", ", &save)) {
        const char *key = token;
        if (strcmp(token, "suggest") == 0) {
            key = "suggestions";
        } else if (strcmp(token, "hintcommit") == 0) {
            key = "english_hint_commit";
        } else if (strcmp(token, "easysymbol") == 0) {
            key = "easy_symbol";
        } else if (strcmp(token, "shift") == 0) {
            key = "shift_toggle";
        }
        set_migrated(out, key, "1");
    }
}

static void read_file(chewing_plus_ibus_prefs *out, const char *path) {
    FILE *file = fopen(path, "r");
    char line[256];
    if (!file) {
        return;
    }
    while (fgets(line, sizeof(line), file)) {
        char *eq;
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        eq = strchr(line, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        eq[1 + strcspn(eq + 1, "\r\n")] = '\0';
        set_migrated(out, line, eq + 1);
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
    size_t i;
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
    fprintf(file, "# 新酷音 Plus (IBus). Keys: chewing_plus_setting_info.\n");
    for (i = 0; i < in->count; i++) {
        fprintf(file, "%s=%s\n", in->items[i].key, in->items[i].value);
    }
    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}
