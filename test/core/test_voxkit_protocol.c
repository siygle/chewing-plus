/*
 * SPDX-License-Identifier: MIT
 *
 * Protocol fixtures taken from fcitx5-voxkit protocol.rs / VoiceClient.cpp.
 */
#include "voxkit_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(int ok, const char *what) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

static void test_roundtrip(void) {
    char buf[512];
    char err[128];
    voxkit_message msg;
    const char *words[] = {"小麥注音", "OpenCode"};
    int n;

    n = voxkit_proto_write_ping(buf, sizeof(buf), 4);
    check(n > 0, "ping writes");
    check(strcmp(buf, "{\"v\":1,\"type\":\"ping\",\"req\":4}\n") == 0,
          "ping bytes");
    check(voxkit_proto_parse(buf, &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "ping parses");
    check(msg.type == VOXKIT_MSG_PING && msg.req == 4 && msg.version == 1,
          "ping fields");

    n = voxkit_proto_write_start(buf, sizeof(buf), 7, NULL, "s2twp", NULL,
                                 words, 2);
    check(n > 0, "start writes");
    check(voxkit_proto_parse(buf, &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "start parses");
    check(msg.type == VOXKIT_MSG_START && msg.req == 7, "start req");
    check(strcmp(msg.language, "zh") == 0, "start language default");
    check(strcmp(msg.convert, "s2twp") == 0, "start convert");

    check(voxkit_proto_parse(
              "{\"v\":1,\"type\":\"final\",\"req\":3,\"text\":\"你好\"}\n",
              &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "daemon final parses");
    check(msg.type == VOXKIT_MSG_FINAL && strcmp(msg.text, "你好") == 0,
          "final text");

    check(voxkit_proto_parse(
              "{\"v\":1,\"type\":\"level\",\"req\":1,\"rms_dbfs\":-22.5}", &msg,
              err, sizeof(err)) == VOXKIT_PROTO_OK,
          "level parses");
    check(msg.has_rms && msg.rms_dbfs < -22.0 && msg.rms_dbfs > -23.0,
          "level value");

    check(voxkit_proto_parse("{\"v\":1,\"type\":\"error\",\"req\":1,\"code\":"
                             "\"model-not-ready\","
                             "\"message\":\"run: voxkit model install\"}",
                             &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "error parses");
    check(strcmp(msg.code, "model-not-ready") == 0, "error code");

    check(voxkit_proto_parse("{\"v\":1,\"type\":\"discarded\",\"req\":1,"
                             "\"reason\":\"no_speech\"}",
                             &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "discarded parses");
    check(strcmp(msg.reason, "no_speech") == 0, "discard reason");

    check(voxkit_proto_parse("{\"type\":\"pong\",\"req\":4,\"ignored\":true}",
                             &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "missing v and unknown field");
    check(msg.version == 0 && msg.type == VOXKIT_MSG_PONG, "omitted v is 0");

    check(voxkit_proto_parse("{\"v\":9,\"type\":\"ping\"}", &msg, err,
                             sizeof(err)) == VOXKIT_PROTO_ERR_VERSION,
          "version 9 rejected");
    check(voxkit_proto_parse("not json", &msg, err, sizeof(err)) ==
              VOXKIT_PROTO_ERR_PARSE,
          "garbage rejected");
    check(voxkit_proto_parse("{\"v\":1,\"type\":\"nope\"}", &msg, err,
                             sizeof(err)) == VOXKIT_PROTO_ERR_TYPE,
          "unknown type rejected");
}

static void test_socket(void) {
    char buf[256];
    setenv("VOXKIT_SOCKET", "/run/user/1/voxkit.sock", 1);
    check(voxkit_proto_default_socket(buf, sizeof(buf)) == VOXKIT_PROTO_OK,
          "socket from env");
    check(strcmp(buf, "/run/user/1/voxkit.sock") == 0, "socket path");
    unsetenv("VOXKIT_SOCKET");
}

int main(void) {
    test_roundtrip();
    test_socket();
    printf(failures ? "PROTOCOL TESTS FAILED (%d)\n" : "PROTOCOL TESTS OK\n",
           failures);
    return failures ? 1 : 0;
}
