/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Plus features for fcitx5-chewing, ported from fcitx5-mcbopomofo-plus.
 * Implemented on top of the public libchewing C API only (option A): the
 * engine is never patched. Everything is off by default.
 */
#ifndef _FCITX5_CHEWING_PLUS_CHEWINGPLUS_H_
#define _FCITX5_CHEWING_PLUS_CHEWINGPLUS_H_

#include <chewing.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "PlusDoubleTap.h"
#include "PlusEmoji.h"
#include "PlusEnglish.h"

#include <memory>

namespace chewing_plus {

struct Options {
    bool emoji = false;          // emoji after phrases (開心 -> 開心😊)
    bool calculator = false;     // "=" on an empty buffer
    bool spaceLetters = false;   // 你好Linux -> 你好 Linux
    bool spaceDigits = false;    // 第5章 -> 第 5 章
    bool emojiShortcode = false; // ;;smile
    EmojiPrefix emojiPrefix = EmojiPrefix::Parse(";;");
    int emojiDoubleTapMs = 300;
    bool english = false;     // [EN] buffer mode
    bool englishHint = false; // "Ctrl+Shift+Space -> GitHub"
    // The shortcut commits the hinted word directly instead of opening the
    // [EN] panel with the reading converted.
    bool englishHintCommit = false;
    std::string englishShortcutLabel = "Control+Shift+Space";
};

// Modal panel shown by the frontend (candidates + preedit).
struct Panel {
    enum class Kind { None, Calculator, EmojiShortcode, English };
    Kind kind = Kind::None;
    std::string input;               // what the user typed in the panel
    std::string preedit;             // full preedit to display
    size_t cursor = 0;               // byte offset in preedit
    std::vector<std::string> labels; // candidate display text
    std::vector<std::string> values; // text committed when chosen
    bool active() const { return kind != Kind::None; }
};

// Result of a Plus key handler.
enum class Handled {
    No,  // fall through to the normal Chewing path
    Yes, // consumed; frontend refreshes UI from panel()/context
};

class ChewingPlus {
public:
    using Commit = std::function<void(const std::string &)>;

    void setOptions(const Options &o) { options_ = o; }
    const Options &options() const { return options_; }
    // Read-only dictionaries, usually shared with other sessions
    // (DataCache). Never null.
    const EmojiShortcodes &shortcodes() const { return *shortcodes_; }
    const EnglishLexicon &lexicon() const { return *lexicon_; }
    void setShortcodes(std::shared_ptr<const EmojiShortcodes> s) {
        shortcodes_ = s ? std::move(s) : std::make_shared<EmojiShortcodes>();
    }
    void setLexicon(std::shared_ptr<const EnglishLexicon> l) {
        lexicon_ = l ? std::move(l) : std::make_shared<EnglishLexicon>();
    }
    const Panel &panel() const { return panel_; }

    // Wall clock for double taps (ms). Tests drive it directly.
    void setNowMs(int64_t ms) { nowMs_ = ms; }

    // Called for every key press (before Chewing sees it). `ascii` is the
    // printable character or 0. Returns Yes when Plus consumed the key.
    Handled onKey(ChewingContext *ctx, char ascii, bool ctrl, bool isBackspace,
                  bool isEscape, bool isReturn, bool isSpace,
                  const Commit &commit);

    // Shortcut (combo or double tap): enter / leave English mode, or convert
    // the unfinished reading if it looks like English.
    bool toggleEnglish(ChewingContext *ctx, const Commit &commit);

    // Dictation from the voxkit module: English-only text goes into the
    // Chewing buffer (stays editable); anything else is committed with the
    // composition around it, like emoji. Closes any open Plus panel first.
    void insertDictation(ChewingContext *ctx, const std::string &text,
                         const Commit &commit);

    // Select candidate i of the open panel.
    void select(ChewingContext *ctx, int i, const Commit &commit);

    // Candidate list for a Chewing candidate window: Plus may append an emoji
    // choice. Returns the extra (display, commit) entry, if any.
    std::optional<std::pair<std::string, std::string>>
    emojiForCandidates(ChewingContext *ctx) const;

    // Text shown under the preedit while typing Zhuyin, or "".
    std::string englishHint(ChewingContext *ctx) const;

    // Initials typed one per syllable (ㄍㄒㄈㄘ): libchewing keeps only the
    // last initial in its reading, so the sequence is tracked here. Empty
    // once the reading holds anything but a lone initial.
    const std::string &typedInitials(ChewingContext *) const {
        return initials_;
    }
    // Buffer length (characters) when the initials began. With toneless
    // typing libchewing turns each initial into a character as it goes
    // (ㄍㄒ -> 更); those belong to the initials, not to the text.
    size_t initialsStart() const { return initialsStart_; }
    // Drop characters toneless typing added after `start`, and the reading.
    static void dropAfter(ChewingContext *ctx, size_t start);

    // Raw-key tracking for the hint: call after Chewing handled a key.
    void afterChewingKey(ChewingContext *ctx, char ascii, bool isBackspace);

    // Auto spacing applied to every commit string.
    std::string postProcessCommit(const std::string &text) const;

    void reset() {
        panel_ = {};
        keys_.clear();
        overwrites_ = 0;
        overwriteFlags_.clear();
        lastZuinLen_ = 0;
        initials_.clear();
        prefixPendingMs_ = -1;
    }

private:
    void openPanel(ChewingContext *ctx, Panel::Kind kind, std::string input);
    void refreshPanel(ChewingContext *ctx);
    void closePanel() { panel_ = {}; }
    // Text of the Chewing buffer split at the cursor (UTF-8).
    static std::pair<std::string, std::string> splitBuffer(ChewingContext *ctx);
    // Commit everything: buffer head + text + tail, then clear Chewing.
    void commitAround(ChewingContext *ctx, const std::string &middle,
                      const Commit &commit);

    Options options_;
    std::shared_ptr<const EmojiShortcodes> shortcodes_ =
        std::make_shared<EmojiShortcodes>();
    std::shared_ptr<const EnglishLexicon> lexicon_ =
        std::make_shared<EnglishLexicon>();
    Panel panel_;
    int64_t nowMs_ = 0;
    // Hint tracking: raw keys of the current reading.
    std::string keys_;
    int overwrites_ = 0;
    std::vector<bool> overwriteFlags_; // per key, for Backspace
    size_t lastZuinLen_ = 0;
    std::string initials_;
    size_t initialsStart_ = 0;
    size_t keysStart_ = 0; // buffer length when keys_ began
    // Emoji double tap.
    int64_t prefixPendingMs_ = -1;
    std::string prefixPendingBuffer_;
    std::string prefixPendingZuin_;
};

} // namespace chewing_plus

#endif
