/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Plus integration tests: drives the real addon through fcitx5's test
 * frontend. Run by test/run-plus-test.sh with an isolated HOME and data dirs
 * so the user's real Chewing dictionary is never touched.
 */
#include "testdir.h"
#include "testfrontend_public.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <string>

using namespace fcitx;

namespace {
int failures = 0;
void check(bool ok, const std::string &what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) {
        ++failures;
    }
}
} // namespace

void testPlus(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *chewing = instance->addonManager().addon("chewing", true);
        FCITX_ASSERT(chewing);
        RawConfig config;
        config.setValueByPath("Layout", "Default Keyboard");
        config.setValueByPath("PlusEmoji", "True");
        config.setValueByPath("PlusCalculator", "True");
        config.setValueByPath("PlusSpaceLetters", "True");
        config.setValueByPath("PlusSpaceDigits", "True");
        config.setValueByPath("PlusEmojiShortcode", "True");
        config.setValueByPath("PlusEmojiShortcodePrefix", ";;");
        config.setValueByPath("PlusEnglish", "True");
        config.setValueByPath("PlusEnglishHint", "True");
        config.setValueByPath("PlusEnglishDoubleTapKeys/0", "Shift_R");
        chewing->setConfig(config);

        auto group = instance->inputMethodManager().currentGroup();
        group.inputMethodList().clear();
        group.inputMethodList().push_back(InputMethodGroupItem("keyboard-us"));
        group.inputMethodList().push_back(InputMethodGroupItem("chewing"));
        group.setDefaultInputMethod("");
        instance->inputMethodManager().setGroup(group);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = tf->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        // Collect commits instead of pre-registering expectations.
        std::string committed;
        auto conn = ic->watch();
        auto *handler = instance->watchEvent(
                                    EventType::InputContextCommitString,
                                    EventWatcherPhase::PreInputMethod,
                                    [&committed, ic](Event &e) {
                                        auto &ce = static_cast<CommitStringEvent &>(e);
                                        if (ce.inputContext() == ic) {
                                            committed += ce.text();
                                        }
                                    })
                            .release();
        auto send = [&](const Key &k, bool release = false) {
            tf->call<ITestFrontend::keyEvent>(uuid, k, release);
        };
        auto key = [&](const Key &k) {
            send(k);
            send(k, true);
        };
        auto type = [&](const std::string &s) {
            for (char c : s) {
                key(Key(static_cast<KeySym>(static_cast<unsigned char>(c))));
            }
        };
        auto preedit = [&]() { return ic->inputPanel().preedit().toString(); };
        auto aux = [&]() { return ic->inputPanel().auxDown().toString(); };
        auto cand = [&](int i) -> std::string {
            auto l = ic->inputPanel().candidateList();
            return l && i < l->size() ? l->candidate(i).text().toString() : "";
        };
        auto reset = [&]() {
            key(Key(FcitxKey_Escape));
            key(Key(FcitxKey_Escape));
            ic->reset();
            committed.clear();
        };
        key(Key("Control+space"));
        check(instance->inputMethod(ic) == "chewing", "chewing active");

        // --- baseline: plain Zhuyin still works (你好 = su3 cl3)
        reset();
        type("su3cl3");
        check(preedit() == "你好", "baseline 你好 preedit, got " + preedit());
        key(Key(FcitxKey_Return));
        check(committed == "你好", "baseline commit 你好, got " + committed);

        // --- calculator
        reset();
        type("=1+2*3");
        check(preedit() == "[計算器] =1+2*3", "calculator preedit, got " + preedit());
        check(cand(0) == "7", "calculator result 7, got " + cand(0));
        key(Key(FcitxKey_Return));
        check(committed == "7", "calculator commits 7, got " + committed);

        // --- auto spacing on commit
        reset();
        type("su3cl3");
        key(Key(FcitxKey_A, KeyState::Shift));
        key(Key(FcitxKey_Return));
        check(committed == "你好 A", "auto space 你好 A, got [" + committed + "]");

        // --- emoji shortcode: ;; then smile, Space commits; Chinese kept
        reset();
        type("su3cl3");
        type(";;");
        check(preedit() == "你好;;", "double tap ;; opens panel keeping 你好, got " + preedit());
        type("thumbsup");
        check(cand(0) == "👍 :thumbsup", "thumbsup first, got " + cand(0));
        key(Key(FcitxKey_space));
        check(committed == "你好👍", "commits 你好👍, got " + committed);

        // --- ; alone stays Bopomofo (ㄤ)
        reset();
        type("w;");
        check(preedit() == "ㄊㄤ", "w; stays ㄊㄤ, got " + preedit());

        // --- English mode via Ctrl+Shift+Space, Chinese kept, casing fixed
        reset();
        type("su3cl3");
        key(Key("Control+Shift+space"));
        check(preedit() == "你好[EN] ", "EN mode keeps 你好, got " + preedit());
        type("github");
        check(cand(0) == "GitHub", "github -> GitHub, got " + cand(0));
        key(Key(FcitxKey_space));
        check(preedit() == "你好GitHub ", "English inserted into buffer, got [" + preedit() + "]");
        key(Key(FcitxKey_Return));
        check(committed == "你好 GitHub ", "commit with spacing, got [" + committed + "]");

        // --- Enter = as typed; Ctrl+2 picks 2nd
        reset();
        key(Key("Control+Shift+space"));
        type("github");
        key(Key(FcitxKey_Return));
        key(Key(FcitxKey_Return));
        check(committed == "github", "Enter as typed, got " + committed);
        reset();
        key(Key("Control+Shift+space"));
        type("iphone");
        key(Key(FcitxKey_2, KeyState::Ctrl, 11));
        key(Key(FcitxKey_Return));
        check(committed == "iphone", "Ctrl+2 picks 2nd (as typed), got " + committed);

        // --- digits and symbols are typed in EN mode
        reset();
        key(Key("Control+Shift+space"));
        type("k8s");
        key(Key(FcitxKey_at, KeyState::Shift));
        check(preedit() == "[EN] k8s@", "digits/@ typed, got " + preedit());

        // --- hint while typing English as Zhuyin; shortcut converts
        reset();
        type("su3");
        type("github");
        check(aux() == "Control+Shift+Space / Right Shift ×2 → GitHub",
              "hint shown, got [" + aux() + "]");
        key(Key("Control+Shift+space"));
        check(preedit() == "你[EN] github", "hint converts keys, got " + preedit());
        key(Key(FcitxKey_space));
        key(Key(FcitxKey_Return));
        check(committed == "你 GitHub ", "converted commit, got [" + committed + "]");

        // --- no hint for real Zhuyin
        reset();
        type("vu0");
        check(aux().empty(), "no hint for ㄒㄧㄢ, got [" + aux() + "]");

        // --- Right Shift x2 toggles English mode
        reset();
        auto tapShift = [&]() {
            send(Key(FcitxKey_Shift_R));
            send(Key(FcitxKey_Shift_R, KeyState::Shift), true);
        };
        tapShift();
        tapShift();
        check(preedit() == "[EN] ", "Right Shift x2 enters EN, got " + preedit());

        // --- emoji suggestion after phrase (開心 = d9 vup)
        reset();
        type("d9 vup ");
        key(Key(FcitxKey_Down));
        bool found = false;
        for (int i = 0; i < 5; i++) {
            found |= cand(i) == "開心😊";
        }
        check(found, "開心😊 offered among candidates");

        // --- password field: no Plus
        reset();
        ic->setCapabilityFlags(CapabilityFlag::Password);
        type("=");
        check(preedit().find("計算器") == std::string::npos, "no Plus in password field");
        ic->setCapabilityFlags(CapabilityFlags{});

        delete handler;
        std::printf(failures ? "PLUS TESTS FAILED (%d)\n" : "PLUS TESTS OK\n",
                    failures);
        instance->exit();
    });
}

int main() {
    // Plus data dir is passed by run-plus-test.sh (setupTestingEnvironment
    // replaces the fcitx data dirs, so it must be listed explicitly).
    const char *plusData = std::getenv("PLUS_TEST_FCITX_DATA");
    std::vector<std::string> dataDirs{TESTING_BINARY_DIR "/test"};
    if (plusData) {
        dataDirs.emplace_back(plusData);
    }
    setupTestingEnvironment(TESTING_BINARY_DIR, {TESTING_BINARY_DIR "/src"},
                            dataDirs);
    char arg0[] = "testplus";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testim,testfrontend,chewing";
    char *argv[] = {arg0, arg1, arg2};
    Log::setLogRule("default=3");
    Instance instance(FCITX_ARRAY_SIZE(argv), argv);
    instance.addonManager().registerDefaultLoader(nullptr);
    testPlus(&instance);
    instance.exec();
    return failures ? 1 : 0;
}
