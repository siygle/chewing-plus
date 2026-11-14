/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Chewing + fcitx5-voxkit end to end, against a fake voxkit daemon
 * (fcitx5-voxkit's test/fake_voxkit.py). Run by test/run-voxkit-test.sh in
 * an isolated HOME. Driven by a timer so the daemon socket is serviced.
 */
#include "testdir.h"
#include "testfrontend_public.h"
#include <cstdio>
#include <cstdlib>
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace fcitx;

namespace {
int failures = 0;
void check(bool ok, const std::string &what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) {
        ++failures;
    }
}

struct H {
    Instance *instance = nullptr;
    AddonInstance *tf = nullptr;
    ICUUID uuid{};
    InputContext *ic = nullptr;
    std::string committed;
    std::unique_ptr<HandlerTableEntry<EventHandler>> watch;
    std::unique_ptr<EventSourceTime> timer;
    int step = 0;
    void send(const Key &k, bool release = false) {
        tf->call<ITestFrontend::keyEvent>(uuid, k, release);
    }
    void key(const Key &k) {
        send(k);
        send(k, true);
    }
    void type(const std::string &s) {
        for (char c : s) {
            key(Key(static_cast<KeySym>(static_cast<unsigned char>(c))));
        }
    }
    std::string preedit() const {
        return ic->inputPanel().preedit().toString();
    }
};

const Key kAltR(FcitxKey_Alt_R);
const Key kAltRUp(FcitxKey_Alt_R, KeyState::Alt);

bool tick(H &h) {
    const std::string text = std::getenv("FAKE_TEXT");
    switch (h.step++) {
    case 0: // empty buffer: dictation commits directly
        h.send(kAltR);
        break;
    case 4:
        h.send(kAltRUp, true);
        break;
    case 8:
        check(h.committed == text,
              "empty buffer: commits dictation, got [" + h.committed + "]");
        h.committed.clear();
        h.type("su3cl3"); // 你好, cursor at end
        h.send(kAltR);
        break;
    case 12:
        h.send(kAltRUp, true);
        break;
    case 16:
        check(h.committed == "你好" + text,
              "composition kept in front, committed together, got [" +
                  h.committed + "]");
        check(h.preedit().empty(), "buffer empty afterwards");
        h.committed.clear();
        // Cursor in the middle: 你|好
        h.type("su3cl3");
        h.key(Key(FcitxKey_Left));
        h.send(kAltR);
        break;
    case 20:
        h.send(kAltRUp, true);
        break;
    case 24:
        check(h.committed == "你" + text + "好",
              "inserted at the cursor, got [" + h.committed + "]");
        h.committed.clear();
        // Half-typed syllable is dropped, not committed as Bopomofo.
        h.type("su");
        h.send(kAltR);
        break;
    case 28:
        h.send(kAltRUp, true);
        break;
    case 32: {
        check(h.committed == text,
              "half-typed ㄋㄧ dropped, got [" + h.committed + "]");
        std::ifstream log(std::getenv("FAKE_LOG"));
        std::stringstream s;
        s << log.rdbuf();
        check(s.str().find("\"type\": \"start\"") != std::string::npos,
              "daemon received start requests");
        h.timer->setEnabled(false);
        std::printf(failures ? "CHEWING VOXKIT TESTS FAILED (%d)\n"
                             : "CHEWING VOXKIT TESTS OK\n",
                    failures);
        h.instance->exit();
        break;
    }
    default:
        break;
    }
    return true;
}
} // namespace

int main() {
    const char *plusData = std::getenv("PLUS_TEST_FCITX_DATA");
    const char *voxkitAddonDir = std::getenv("VOXKIT_ADDON_DIR");
    const char *voxkitDataDir = std::getenv("VOXKIT_DATA_DIR");
    std::vector<std::string> addonDirs{TESTING_BINARY_DIR "/src"};
    std::vector<std::string> dataDirs{TESTING_BINARY_DIR "/test"};
    if (voxkitAddonDir) {
        addonDirs.emplace_back(voxkitAddonDir);
    }
    if (voxkitDataDir) {
        dataDirs.emplace_back(voxkitDataDir);
    }
    if (plusData) {
        dataDirs.emplace_back(plusData);
    }
    setupTestingEnvironment(TESTING_BINARY_DIR, addonDirs, dataDirs);
    char arg0[] = "testvoxkit";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testim,testfrontend,chewing,voxkit";
    char *argv[] = {arg0, arg1, arg2};
    Log::setLogRule("default=3");
    Instance instance(3, argv);
    instance.addonManager().registerDefaultLoader(nullptr);

    H h;
    h.instance = &instance;
    instance.eventDispatcher().schedule([&h]() {
        auto *voxkit = h.instance->addonManager().addon("voxkit", true);
        check(voxkit != nullptr, "voxkit module loads");
        auto *chewing = h.instance->addonManager().addon("chewing", true);
        check(chewing != nullptr, "chewing loads (registers with voxkit)");
        RawConfig config;
        config.setValueByPath("Layout", "Default Keyboard");
        chewing->setConfig(config);
        auto group = h.instance->inputMethodManager().currentGroup();
        group.inputMethodList().clear();
        group.inputMethodList().push_back(InputMethodGroupItem("keyboard-us"));
        group.inputMethodList().push_back(InputMethodGroupItem("chewing"));
        group.setDefaultInputMethod("");
        h.instance->inputMethodManager().setGroup(group);
        h.tf = h.instance->addonManager().addon("testfrontend");
        h.uuid = h.tf->call<ITestFrontend::createInputContext>("testapp");
        h.ic = h.instance->inputContextManager().findByUUID(h.uuid);
        h.key(Key("Control+space"));
        check(h.instance->inputMethod(h.ic) == "chewing", "chewing active");
        h.watch = h.instance->watchEvent(
            EventType::InputContextCommitString,
            EventWatcherPhase::PreInputMethod, [&h](Event &e) {
                auto &ce = static_cast<CommitStringEvent &>(e);
                if (ce.inputContext() == h.ic) {
                    h.committed += ce.text();
                }
            });
        h.timer = h.instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, now(CLOCK_MONOTONIC) + 100000, 0,
            [&h](EventSourceTime *src, uint64_t) {
                src->setNextInterval(100000);
                src->setOneShot();
                return tick(h);
            });
    });
    instance.exec();
    return failures ? 1 : 0;
}
