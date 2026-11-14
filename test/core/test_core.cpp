/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Framework-free tests for the Plus core. No fcitx, no Chewing context.
 */
#include "PlusDoubleTap.h"
#include "PlusEmoji.h"
#include "PlusEnglish.h"
#include "PlusFeatures.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string &what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) {
        ++failures;
    }
}

std::string TempFile(const std::string &body) {
    char path[] = "/tmp/chewing-plus-core-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        return {};
    }
    FILE *f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        return {};
    }
    std::fputs(body.c_str(), f);
    std::fclose(f);
    return path;
}

void testCalculator() {
    using chewing_plus::Calculate;
    auto is = [](const char *expr, const char *want) {
        auto got = Calculate(expr);
        check(got && *got == want,
              std::string("calc ") + expr + " -> " + (got ? *got : "(none)"));
    };
    is("1+2*3", "7");
    is("(2+3)*4", "20");
    is("1/2", "0.5");
    is("-3+5", "2");
    is(" 10 % 4 ", "2");
    check(!Calculate("1/0"), "division by zero rejected");
    check(!Calculate(""), "empty expression rejected");
    check(!Calculate("1+"), "trailing operator rejected");
}

void testAutoSpace() {
    const std::string in = "你好Linux第5章";
    check(chewing_plus::AutoSpace(in, true, true) == "你好 Linux 第 5 章",
          "auto space letters and digits");
    check(chewing_plus::AutoSpace(in, true, false) == "你好 Linux 第5章",
          "auto space letters only");
    check(chewing_plus::AutoSpace(in, false, true) == "你好Linux第 5 章",
          "auto space digits only");
    check(chewing_plus::AutoSpace(in, false, false) == in, "auto space off");
}

void testEmojiForText() {
    auto emoji = chewing_plus::EmojiForText("今天開心");
    check(!emoji.empty() && emoji.front() == "今天開心😊",
          "emoji candidate for 開心");
    check(chewing_plus::EmojiForText("你好").empty(), "no emoji for 你好");
}

void testShortcodes() {
    chewing_plus::EmojiShortcodes codes;
    check(codes.load(PLUS_EMOJI_PATH) > 1000, "built-in shortcodes load");
    auto hit = codes.search("thumbsup");
    check(!hit.empty() && hit.front().emoji == "👍" &&
              hit.front().code == "thumbsup",
          "thumbsup -> 👍");
    auto path = TempFile("zztest\t🧪\tcustom\n");
    check(!path.empty() && codes.load(path, true) == 1, "user shortcode loads");
    auto user = codes.search("zztest");
    check(!user.empty() && user.front().emoji == "🧪", "user shortcode wins");
    auto quick = codes.search("");
    check(!quick.empty() && quick.front().code == "zztest",
          "empty query lists user shortcodes");
    if (!path.empty()) {
        unlink(path.c_str());
    }

    auto semi = chewing_plus::EmojiPrefix::Parse(";;");
    check(semi.valid() && semi.doubleTap && semi.key == ';', "prefix ;;");
    auto single = chewing_plus::EmojiPrefix::Parse("`");
    check(single.valid() && !single.doubleTap && single.key == '`', "prefix `");
    check(!chewing_plus::EmojiPrefix::Parse("a").valid(),
          "letter prefix rejected");
    check(!chewing_plus::EmojiPrefix::Parse(";:").valid(),
          "mismatched double prefix rejected");
}

void testEnglish() {
    chewing_plus::EnglishLexicon lex;
    check(lex.load(PLUS_ENGLISH_PATH) > 1000, "built-in English lexicon loads");
    check(lex.contains("github"), "lexicon contains github");
    check(lex.canonical("github") == "GitHub", "github -> GitHub");
    auto cands = lex.candidates("github");
    check(!cands.empty() && cands.front() == "GitHub",
          "github candidate is GitHub");
    check(lex.canonical("NotAWordZZZ") == "NotAWordZZZ",
          "unknown word stays as typed");

    auto path = TempFile("OpenVanilla\nk8s\n");
    check(!path.empty() && lex.load(path, true) == 2,
          "user English words load");
    check(lex.canonical("openvanilla") == "OpenVanilla", "user casing wins");
    auto userCands = lex.candidates("k8s");
    check(!userCands.empty() && userCands.front() == "k8s", "k8s candidate");
    if (!path.empty()) {
        unlink(path.c_str());
    }
}

void testDoubleTap() {
    chewing_plus::DoubleTap tap;
    tap.setWindowMs(300);
    check(!tap.onKey(true, false, 0), "first press is not a double tap");
    check(!tap.onKey(true, true, 40), "first release is not a double tap");
    check(!tap.onKey(true, false, 80), "second press is not a double tap");
    check(tap.onKey(true, true, 100), "second release fires");

    tap.reset();
    check(!tap.onKey(true, false, 0), "press");
    check(!tap.onKey(true, true, 20), "release");
    check(!tap.onKey(false, false, 40), "another key cancels");
    check(!tap.onKey(true, false, 60), "press after cancel");
    check(!tap.onKey(true, true, 80), "release after cancel is a first tap");

    tap.reset();
    check(!tap.onKey(true, false, 0), "long press");
    check(!tap.onKey(true, true, 500), "long hold is not a tap");
    check(!tap.onKey(true, false, 520), "press");
    check(!tap.onKey(true, true, 540), "no pending first tap");

    tap.reset();
    check(!tap.onKey(true, false, 0), "shift down");
    check(!tap.onKey(false, false, 10), "shift+other is not a tap");
    check(!tap.onKey(false, true, 20), "other up");
    check(!tap.onKey(true, true, 30), "shift up after another key");

    tap.reset();
    tap.setWindowMs(300);
    check(!tap.onKey(true, false, 0), "press");
    check(!tap.onKey(true, true, 10), "release");
    check(!tap.onKey(true, false, 310), "press at the window edge");
    check(tap.onKey(true, true, 320), "edge is inside the window");
    tap.reset();
    check(!tap.onKey(true, false, 0), "press");
    check(!tap.onKey(true, true, 10), "release");
    check(!tap.onKey(true, false, 311), "press just outside the window");
    check(!tap.onKey(true, true, 320), "outside the window does not fire");
}

} // namespace

int main() {
    testCalculator();
    testAutoSpace();
    testEmojiForText();
    testShortcodes();
    testEnglish();
    testDoubleTap();
    std::printf(failures ? "CORE TESTS FAILED (%d)\n" : "CORE TESTS OK\n",
                failures);
    return failures ? 1 : 0;
}
