#ifndef CHEWING_PLUS_BRIDGING_HEADER_H_
#define CHEWING_PLUS_BRIDGING_HEADER_H_

#include "chewing_plus.h"
#include "voxkit_client.h"
#include "voxkit_protocol.h"

#include <string.h>

static inline void chewing_plus_mac_zero_message(voxkit_message *message) {
    if (message) {
        memset(message, 0, sizeof(*message));
    }
}

static inline const char *
chewing_plus_mac_message_text(const voxkit_message *message) {
    return message ? message->text : "";
}

static inline const char *
chewing_plus_mac_message_tentative(const voxkit_message *message) {
    return message ? message->tentative : "";
}

static inline const char *
chewing_plus_mac_message_error(const voxkit_message *message) {
    return message ? message->message : "";
}

static inline const char *
chewing_plus_mac_model_id(const voxkit_model_info *model) {
    return model ? model->id : "";
}

static inline const char *
chewing_plus_mac_model_kind(const voxkit_model_info *model) {
    return model ? model->kind : "";
}

static inline const char *
chewing_plus_mac_daemon(const voxkit_message *message) {
    return message ? message->daemon : "";
}

static inline const char *
chewing_plus_mac_state(const voxkit_message *message) {
    return message ? message->state : "";
}

static inline const char *
chewing_plus_mac_store(const voxkit_message *message) {
    return message ? message->store : "";
}

static inline const char *chewing_plus_mac_note(const voxkit_message *message) {
    return message ? message->message : "";
}

static inline const char *
chewing_plus_mac_active(const voxkit_message *message) {
    return message ? message->model : "";
}

static inline voxkit_message chewing_plus_mac_blank_message(void) {
    voxkit_message message;
    memset(&message, 0, sizeof(message));
    return message;
}

static inline voxkit_model_info chewing_plus_mac_blank_model(void) {
    voxkit_model_info model;
    memset(&model, 0, sizeof(model));
    return model;
}

#endif
