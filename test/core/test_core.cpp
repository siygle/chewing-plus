/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Framework-free tests for the Plus core. No fcitx, no Chewing context.
 */
#include "PlusData.h"
#include "PlusDoubleTap.h"
#include "PlusEmoji.h"
#include "PlusEnglish.h"
#include "PlusFeatures.h"
#include "PlusPhrases.h"
#include "PlusUserHistory.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>
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

void testNeedsSpaceBeforeEnglish() {
    using chewing_plus::NeedsSpaceBeforeEnglish;
    check(NeedsSpaceBeforeEnglish("你好"), "Han before English: space");
    check(NeedsSpaceBeforeEnglish("abc你"), "only the last character counts");
    check(NeedsSpaceBeforeEnglish("𠀀"), "Han extension B (4 bytes): space");
    check(NeedsSpaceBeforeEnglish("ㄅ"), "Bopomofo: space");
    check(NeedsSpaceBeforeEnglish("かな"), "kana: space");
    check(!NeedsSpaceBeforeEnglish(""), "start of text: none");
    check(!NeedsSpaceBeforeEnglish("你好 "), "after a space: none");
    check(!NeedsSpaceBeforeEnglish("你好\n"), "after a newline: none");
    check(!NeedsSpaceBeforeEnglish("hello"), "after English: none");
    check(!NeedsSpaceBeforeEnglish("第5"), "after a digit: none");
    check(!NeedsSpaceBeforeEnglish("你好，"), "after full-width comma: none");
    check(!NeedsSpaceBeforeEnglish("你好。"), "after full-width stop: none");
    check(!NeedsSpaceBeforeEnglish("「"), "after a corner bracket: none");
    check(!NeedsSpaceBeforeEnglish("你\xe5"),
          "truncated UTF-8: none, no crash");
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
    check(lex.load(PLUS_ENGLISH_PATH) > 80000,
          "built-in English lexicon loads");
    check(lex.contains("github"), "lexicon contains github");
    check(lex.canonical("github") == "GitHub", "github -> GitHub");
    check(lex.canonical("macos") == "macOS", "macos -> macOS");
    check(lex.canonical("chatgpt") == "ChatGPT", "chatgpt -> ChatGPT");
    check(lex.canonical("gitlab") == "GitLab", "gitlab -> GitLab");
    check(lex.canonical("kubernetes") == "Kubernetes",
          "kubernetes -> Kubernetes");
    check(lex.canonical("hsinchu") == "Hsinchu", "hsinchu -> Hsinchu");
    check(lex.canonical("tsmc") == "TSMC", "tsmc -> TSMC");
    check(lex.canonical("json") == "JSON", "json -> JSON");
    check(lex.contains("frontend"), "lexicon contains frontend");
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

void testCompiledPhrases() {
    chewing_plus::PhraseIndex text;
    text.load(PLUS_PHRASES_PATH);
    text.finalize();
    char path[] = "/tmp/chewing-plus-phrases-XXXXXX";
    int fd = mkstemp(path);
    check(fd >= 0, "temp file");
    close(fd);
    check(text.writeCompiled(path), "write compiled phrases");
    chewing_plus::PhraseIndex bin;
    check(bin.load(path) == text.size() && bin.compiled(),
          "compiled file loads with the same count");
    bool same = true;
    for (const char *q : {"中華", "台灣", "電腦", "新酷", "學生", "謝謝"}) {
        same &= bin.complete(q) == text.complete(q);
    }
    for (const char *q : {"ㄍㄒㄈㄘ", "ㄓㄏㄇㄍ", "ㄊㄑㄩ", "ㄒㄒㄒ"}) {
        same &= bin.expand(q) == text.expand(q);
    }
    check(same, "compiled and text give identical results");
    // A truncated file must be rejected, not read past its end.
    {
        std::ofstream cut(path, std::ios::binary | std::ios::trunc);
        cut.write("CPPHRS01\xff\xff\x00\x00", 12);
    }
    chewing_plus::PhraseIndex bad;
    check(!bad.compiled() && bad.load(path) == 0, "truncated file rejected");
    std::remove(path);
}

void testDataCache() {
    namespace dc = chewing_plus::DataCache;
    std::string user = TempFile("Zzzcachetest\n");
    {
        auto a = dc::english(PLUS_ENGLISH_PATH, user);
        auto b = dc::english(PLUS_ENGLISH_PATH, user);
        check(a.get() == b.get(), "two sessions share one English lexicon");
        check(a->contains("zzzcachetest"), "shared lexicon has the user word");
        auto c = dc::english(PLUS_ENGLISH_PATH, "");
        check(c.get() != a.get(), "different files get their own lexicon");
        // Edit the user file: the next lookup must see the change.
        sleep(1);
        {
            std::ofstream out(user);
            out << "Zzzcachetest\nYyycachetest\n";
        }
        auto d = dc::english(PLUS_ENGLISH_PATH, user);
        check(d.get() != a.get() && d->contains("yyycachetest"),
              "an edited user file is reloaded");
        auto p1 = dc::phrases(PLUS_PHRASES_PATH);
        auto p2 = dc::phrases(PLUS_PHRASES_PATH);
        check(p1.get() == p2.get() && p1->size() > 100000,
              "two sessions share one phrase index");
        auto e1 = dc::emoji(PLUS_EMOJI_PATH, "");
        auto e2 = dc::emoji(PLUS_EMOJI_PATH, "");
        check(e1.get() == e2.get() && e1->size() > 1000,
              "two sessions share one emoji table");
    }
    check(dc::liveCount() == 0, "dictionaries are freed with their last user");
    std::remove(user.c_str());
}

void testPhrases() {
    chewing_plus::PhraseIndex index;
    check(index.load(PLUS_PHRASES_PATH) > 100000, "phrase index loads");
    index.finalize();
    auto c = index.complete("中華");
    check(!c.empty() && c[0] == "中華民國", "中華 completes to 中華民國 first");
    auto e = index.expand("ㄍㄒㄈㄘ");
    check(e.size() == 1 && e[0] == "恭喜發財", "ㄍㄒㄈㄘ expands to 恭喜發財");
    auto mm = index.expand("ㄇㄇ");
    check(std::find(mm.begin(), mm.end(), "媽媽") != mm.end(),
          "ㄇㄇ lists 媽媽");
    check(index.expand("ㄍㄒ", 9, 3).empty(),
          "two initials are not expanded with a minimum of 3");
    check(index.expand("ㄇ").empty(), "one initial is never expanded");
    check(index.complete("中").empty(), "one character is not completed");
    check(chewing_plus::AllBopomofo("ㄍㄒ") &&
              !chewing_plus::AllBopomofo("ㄍa"),
          "AllBopomofo");
}

void testUserHistory() {
    using chewing_plus::UserHistory;
    char path[] = "/tmp/chewing-plus-usage-XXXXXX";
    int fd = mkstemp(path);
    check(fd >= 0, "usage temp file");
    if (fd >= 0) {
        close(fd);
        unlink(path);
    }

    const int64_t t0 = 1795000000;
    {
        UserHistory h;
        h.load(path);
        // Before learning: static freq 15000 beats 50.
        check(h.score(UserHistory::Kind::Phrase, "中華民國", 15000, t0) >
                  h.score(UserHistory::Kind::Phrase, "中華電信", 50, t0),
              "static high freq starts higher");
        // One explicit suggestion pick (+2.0) lifts 中華電信 above 中華民國.
        h.record(UserHistory::Kind::Phrase, "中華電信", "ㄓㄏㄉㄒ", 2.0, t0);
        check(h.score(UserHistory::Kind::Phrase, "中華電信", 50, t0) >
                  h.score(UserHistory::Kind::Phrase, "中華民國", 15000, t0),
              "one explicit pick lifts phrase above high-freq static phrase");
        // After 30 days unused, recency and count decay smoothly back below.
        const int64_t t30 = t0 + 30 * 86400;
        check(h.score(UserHistory::Kind::Phrase, "中華電信", 50, t30) <
                  h.score(UserHistory::Kind::Phrase, "中華民國", 15000, t30),
              "unused phrase decays back below static high-freq after 30 days");
        // Frequently used phrase (10 times) stays on top even after 30 days.
        for (int i = 0; i < 10; ++i) {
            h.record(UserHistory::Kind::Phrase, "中華電信", "ㄓㄏㄉㄒ", 1.0,
                     t0 + i * 60);
        }
        check(h.score(UserHistory::Kind::Phrase, "中華電信", 50, t30) >
                  h.score(UserHistory::Kind::Phrase, "中華民國", 15000, t30),
              "frequently typed phrase stays higher after 30 days");
        h.record(UserHistory::Kind::Char, "妳", "", 2.0, t0);
        check(h.saveIfDirty(t0, true), "usage saves atomically");
    }

    // Reload from disk and verify persistence + multi-process merge.
    {
        UserHistory h1;
        check(h1.load(path) >= 2, "usage reloads from disk");
        check(h1.hasPhrase("中華電信"), "reloaded usage contains 中華電信");

        UserHistory h2;
        h2.load(path);
        h2.record(UserHistory::Kind::Phrase, "新酷音測試", "ㄒㄎㄧㄘㄕ", 2.0,
                  t0 + 100);
        check(h2.saveIfDirty(t0 + 100, true), "second process saves");

        h1.record(UserHistory::Kind::Phrase, "另一個詞", "ㄌㄧㄍㄘ", 2.0,
                  t0 + 120);
        check(h1.saveIfDirty(t0 + 120, true), "first process merges and saves");

        UserHistory h3;
        h3.load(path);
        check(h3.hasPhrase("新酷音測試") && h3.hasPhrase("另一個詞") &&
                  h3.hasPhrase("中華電信"),
              "multi-process merge preserves both processes' records");
        h3.clear();
        check(h3.empty(), "clear empties in-memory history");
    }
    struct stat st{};
    check(stat(path, &st) != 0, "clear removes usage file on disk");
}

void testAdaptivePhrasesAndOverlay() {
    using chewing_plus::PhraseIndex;
    using chewing_plus::UserHistory;

    PhraseIndex base;
    check(base.load(PLUS_PHRASES_PATH) > 100000, "base phrases load");
    base.finalize();
    check(base.containsText("中華民國") && !base.containsText("不存在的詞組"),
          "containsText binary search");

    const int64_t t0 = 1795000000;
    UserHistory hist;
    auto before = base.complete("中華", 9, 2, nullptr, &hist, t0);
    check(!before.empty() && before[0] == "中華民國",
          "adaptive complete starts with 中華民國");
    hist.record(UserHistory::Kind::Phrase, "中華電信", "", 2.0, t0);
    auto after = base.complete("中華", 9, 2, nullptr, &hist, t0);
    check(!after.empty() && after[0] == "中華電信",
          "adaptive complete ranks 中華電信 first after use");
    auto decayed = base.complete("中華", 9, 2, nullptr, &hist, t0 + 30 * 86400);
    check(!decayed.empty() && decayed[0] == "中華民國",
          "adaptive complete decays back to 中華民國 after 30 days");

    // Imported libchewing user phrase surfaces in both complete and expand.
    hist.importUserPhrases({{"中華新酷音", "ㄓㄏㄒㄎㄧ"}});
    auto withImport = base.complete("中華", 9, 2, nullptr, &hist, t0);
    check(std::find(withImport.begin(), withImport.end(), "中華新酷音") !=
              withImport.end(),
          "imported user phrase appears in completions");
    auto expImport = base.expand("ㄓㄏㄒㄎㄧ", 9, 2, nullptr, &hist, t0);
    check(!expImport.empty() && expImport[0] == "中華新酷音",
          "imported user phrase appears in initials expansion");

    // Live chars adaptive re-ranking keeps exact reading ahead of longer ones.
    PhraseIndex chars;
    check(chars.load(PLUS_CHARS_PATH, true) > 10000, "base chars load");
    chars.finalize();
    auto cBefore = chars.byReading("ㄋㄧ", 9, nullptr, &hist, t0);
    check(!cBefore.empty() && cBefore[0] == "你",
          "live chars ㄋㄧ starts with 你");
    hist.record(UserHistory::Kind::Char, "妳", "", 2.0, t0);
    auto cAfter = chars.byReading("ㄋㄧ", 9, nullptr, &hist, t0);
    check(!cAfter.empty() && cAfter[0] == "妳",
          "live chars ㄋㄧ ranks 妳 first after use");

    // User overlay file formats: 3-col, full bopomofo, 2-col, 1-col.
    std::string userFile =
        TempFile("# custom user phrases\n"
                 "生成式人工智慧\t5000\tㄕㄔㄕㄖㄍㄓㄏ\n"
                 "提示詞工程\t5000\tㄊㄧˊ ㄕˋ ㄘˊ ㄍㄨㄥ ㄔㄥˊ\n"
                 "輝達晶片\tㄏㄨㄟ ㄉㄚˊ ㄐㄧㄥ ㄆㄧㄢˋ\n"
                 "純補全詞條\n");
    PhraseIndex extra;
    check(extra.load(userFile, false, true) == 4,
          "user phrase file loads all 4 formats");
    extra.finalize();
    auto uComp = base.complete("生成式", 9, 2, &extra, nullptr, t0);
    check(!uComp.empty() && uComp[0] == "生成式人工智慧",
          "user overlay completes 生成式人工智慧");
    auto uComp1 = base.complete("純補", 9, 2, &extra, nullptr, t0);
    check(!uComp1.empty() && uComp1[0] == "純補全詞條",
          "1-col user overlay completes 純補全詞條");
    auto uExp = base.expand("ㄊㄕㄘㄍㄔ", 9, 2, &extra, nullptr, t0);
    check(!uExp.empty() && uExp[0] == "提示詞工程",
          "full-bopomofo user overlay expands ㄊㄕㄘㄍㄔ");
    auto uExp2 = base.expand("ㄏㄉㄐㄆ", 9, 2, &extra, nullptr, t0);
    check(!uExp2.empty() && uExp2[0] == "輝達晶片",
          "2-col user overlay expands ㄏㄉㄐㄆ");
    unlink(userFile.c_str());
}

} // namespace

int main() {
    testCalculator();
    testAutoSpace();
    testEmojiForText();
    testShortcodes();
    testEnglish();
    testNeedsSpaceBeforeEnglish();
    testDoubleTap();
    testPhrases();
    testCompiledPhrases();
    testDataCache();
    testUserHistory();
    testAdaptivePhrasesAndOverlay();
    std::printf(failures ? "CORE TESTS FAILED (%d)\n" : "CORE TESTS OK\n",
                failures);
    return failures ? 1 : 0;
}
