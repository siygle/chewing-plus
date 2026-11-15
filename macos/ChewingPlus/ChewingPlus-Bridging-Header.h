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

#endif
