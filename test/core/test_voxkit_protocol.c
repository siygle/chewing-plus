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

static void test_model_manager(void) {
    char buf[320];
    char err[128];
    voxkit_message msg;
    voxkit_model_info rows[VOXKIT_MODELS_MAX];
    size_t count = 0;
    const char *line =
        "{\"capabilities\":[\"translate\",\"model-manager\"],\"daemon\":"
        "\"offline\",\"free_bytes\":5000,\"model\":\"whisper-tiny\","
        "\"model_mt\":\"hy-mt1.5-1.8b\",\"models\":[{\"active\":false,"
        "\"blurb\":\"mt\",\"bytes\":1133080512,\"downloading\":true,"
        "\"id\":\"hy-mt1.5-1.8b\",\"installed\":false,\"kind\":\"mt\","
        "\"license\":\"Tencent HY Community License\",\"license_accepted\":"
        "false,\"partial_bytes\":1000,\"purpose\":\"mt\",\"recommended\":"
        "true,\"removable\":false,\"restricted\":true,\"restriction\":"
        "\"不得在歐盟使用\",\"runtime_bytes\":1300000000,\"selectable\":true,"
        "\"size_on_disk\":0,\"source\":\"\",\"title\":\"HY-MT1.5 "
        "1.8B（翻譯）\"}],\"readonly_dirs\":[],\"req\":0,\"store\":\"/s\","
        "\"store_bytes\":0,\"type\":\"model.list\",\"v\":1}";

    check(voxkit_proto_parse(line, &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "manager list parses");
    check(msg.caps_model_manager && msg.caps_translate &&
              msg.free_bytes == 5000 && msg.has_free_bytes &&
              strcmp(msg.model_mt, "hy-mt1.5-1.8b") == 0,
          "manager list summary");
    check(voxkit_proto_parse("{\"v\":1,\"type\":\"model.list\","
                             "\"store\":\"/s\",\"store_bytes\":12}",
                             &msg, err, sizeof(err)) == VOXKIT_PROTO_OK &&
              !msg.has_free_bytes && msg.free_bytes == 0,
          "an older list without free_bytes is marked unreported");
    check(voxkit_proto_parse("{\"v\":1,\"type\":\"model.list\","
                             "\"free_bytes\":0}",
                             &msg, err, sizeof(err)) == VOXKIT_PROTO_OK &&
              msg.has_free_bytes && msg.free_bytes == 0,
          "free_bytes:0 is reported as zero, not missing");
    check(voxkit_proto_parse_models(line, rows, VOXKIT_MODELS_MAX, &count, err,
                                    sizeof(err)) == VOXKIT_PROTO_OK &&
              count == 1,
          "manager rows");
    check(strcmp(rows[0].purpose, "mt") == 0 && rows[0].restricted &&
              !rows[0].license_accepted && rows[0].recommended &&
              rows[0].selectable && rows[0].downloading &&
              rows[0].partial_bytes == 1000 &&
              rows[0].runtime_bytes == 1300000000ULL &&
              strcmp(rows[0].restriction, "不得在歐盟使用") == 0 &&
              strncmp(rows[0].title, "HY-MT1.5", 8) == 0,
          "manager row fields");

    check(voxkit_proto_write_model_install_accept(buf, sizeof(buf), 9,
                                                  "hy-mt1.5-1.8b") > 0 &&
              strstr(buf, "\"accept_license\":true}\n") != NULL,
          "install accept writes");
    check(voxkit_proto_write_model_verify(buf, sizeof(buf), 10, "silero-vad") >
                  0 &&
              voxkit_proto_parse(buf, &msg, err, sizeof(err)) ==
                  VOXKIT_PROTO_OK &&
              msg.type == VOXKIT_MSG_MODEL_VERIFY,
          "verify round trip");
    check(voxkit_proto_parse("{\"daemon\":\"offline\",\"model\":\"silero-vad\","
                             "\"req\":0,\"result\":\"missing\",\"type\":"
                             "\"model.verify\",\"v\":1}",
                             &msg, err, sizeof(err)) == VOXKIT_PROTO_OK &&
              strcmp(msg.result, "missing") == 0,
          "verify result");
}

static void test_models(void) {
    char buf[256];
    char err[128];
    voxkit_message msg;
    voxkit_model_info rows[4];
    size_t count = 0;
    const char *line =
        "{\"v\":1,\"type\":\"model.list\",\"req\":3,\"daemon\":\"running\","
        "\"store\":\"/tmp/"
        "models\",\"store_bytes\":12,\"model\":\"whisper-tiny\","
        "\"models\":[{\"id\":\"whisper-tiny\",\"kind\":\"asr\",\"bytes\":10,"
        "\"size_on_disk\":10,\"installed\":true,\"active\":true,\"blurb\":"
        "\"tiny\"},"
        "{\"id\":\"silero-vad\",\"kind\":\"vad\",\"bytes\":4,\"installed\":"
        "false,"
        "\"active\":false,\"blurb\":\"vad\"}]}";

    check(voxkit_proto_write_model_install(buf, sizeof(buf), 2,
                                           "qwen3-asr-0.6b") > 0,
          "install writes");
    check(voxkit_proto_parse(buf, &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "install parses");
    check(msg.type == VOXKIT_MSG_MODEL_INSTALL &&
              strcmp(msg.model, "qwen3-asr-0.6b") == 0,
          "install model");
    check(voxkit_proto_parse(line, &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "list parses");
    check(msg.type == VOXKIT_MSG_MODEL_LIST && msg.store_bytes == 12 &&
              strcmp(msg.daemon, "running") == 0,
          "list fields");
    check(voxkit_proto_parse_models(line, rows, 4, &count, err, sizeof(err)) ==
              VOXKIT_PROTO_OK,
          "list rows");
    check(count == 2 && rows[0].installed && rows[0].active &&
              strcmp(rows[0].id, "whisper-tiny") == 0 &&
              strcmp(rows[1].kind, "vad") == 0 && !rows[1].installed,
          "row values");
    check(voxkit_proto_parse(
              "{\"v\":1,\"type\":\"model.progress\",\"req\":2,\"model\":"
              "\"whisper-tiny\",\"state\":\"downloading\",\"received\":5,"
              "\"bytes\":10}",
              &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "progress parses");
    check(msg.type == VOXKIT_MSG_MODEL_PROGRESS && msg.received == 5 &&
              msg.bytes == 10 && strcmp(msg.state, "downloading") == 0,
          "progress fields");
}

static void test_socket(void) {
    char buf[256];
    setenv("VOXKIT_SOCKET", "/run/user/1/voxkit.sock", 1);
    check(voxkit_proto_default_socket(buf, sizeof(buf)) == VOXKIT_PROTO_OK,
          "socket from env");
    check(strcmp(buf, "/run/user/1/voxkit.sock") == 0, "socket path");
    unsetenv("VOXKIT_SOCKET");
}

/* Translation extension: mode/target on start, translated finals, and the
 * capability that gates sending mode=translate at all. */
static void test_translate(void) {
    char buf[512];
    char err[128];
    voxkit_message msg;
    int n;

    n = voxkit_proto_write_start_mode(buf, sizeof(buf), 9, "zh", "s2twp", NULL,
                                      NULL, 0, "translate", "ja");
    check(n > 0, "translate start writes");
    check(strstr(buf, "\"mode\":\"translate\"") != NULL &&
              strstr(buf, "\"target\":\"ja\"") != NULL,
          "translate start carries mode and target");
    check(voxkit_proto_parse(buf, &msg, err, sizeof(err)) == VOXKIT_PROTO_OK &&
              strcmp(msg.mode, "translate") == 0 &&
              strcmp(msg.target, "ja") == 0,
          "translate start parses");

    n = voxkit_proto_write_start(buf, sizeof(buf), 10, "zh", "s2twp", NULL,
                                 NULL, 0);
    check(n > 0 && strstr(buf, "\"mode\"") == NULL &&
              strstr(buf, "\"target\"") == NULL,
          "plain start has no mode (old daemons see the same bytes)");

    check(voxkit_proto_parse(
              "{\"v\":1,\"type\":\"final\",\"req\":9,\"text\":\"Hello\","
              "\"mode\":\"translate\",\"target\":\"en\",\"translated\":true,"
              "\"source\":\"你好\"}",
              &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "translated final parses");
    check(msg.translated == 1 && strcmp(msg.text, "Hello") == 0 &&
              strcmp(msg.source, "你好") == 0 &&
              strcmp(msg.mode, "translate") == 0 && msg.translate_error[0] == 0,
          "translated final fields");

    check(voxkit_proto_parse(
              "{\"v\":1,\"type\":\"final\",\"req\":9,\"text\":\"你好\","
              "\"mode\":\"translate\",\"target\":\"en\",\"translated\":false,"
              "\"source\":\"你好\",\"translate_error\":\"plugin-missing\"}",
              &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "fallback final parses");
    check(msg.translated == 0 && strcmp(msg.text, "你好") == 0 &&
              strcmp(msg.translate_error, "plugin-missing") == 0,
          "fallback final keeps the original text and the reason");

    check(voxkit_proto_parse(
              "{\"v\":1,\"type\":\"model.status\",\"req\":0,\"state\":\"idle\","
              "\"capabilities\":[\"translate\"],\"translate\":{\"plugin\":true,"
              "\"model\":\"hy-mt1.5-1.8b\",\"installed\":false,"
              "\"targets\":[\"en\",\"ja\"]}}",
              &msg, err, sizeof(err)) == VOXKIT_PROTO_OK,
          "status with capabilities parses");
    check(msg.caps_translate == 1, "translate capability seen");
    check(
        voxkit_proto_parse(
            "{\"v\":1,\"type\":\"model.status\",\"req\":0,\"state\":\"idle\"}",
            &msg, err, sizeof(err)) == VOXKIT_PROTO_OK &&
            msg.caps_translate == 0,
        "old daemon status: no translate capability");
    check(voxkit_proto_parse("{\"v\":1,\"type\":\"model.status\",\"req\":0,"
                             "\"capabilities\":[\"streaming\",1,{\"x\":2}]}",
                             &msg, err, sizeof(err)) == VOXKIT_PROTO_OK &&
              msg.caps_translate == 0,
          "other capabilities are ignored");
}

int main(void) {
    test_roundtrip();
    test_translate();
    test_models();
    test_model_manager();
    test_socket();
    printf(failures ? "PROTOCOL TESTS FAILED (%d)\n" : "PROTOCOL TESTS OK\n",
           failures);
    return failures ? 1 : 0;
}
