/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "ChewingPlus.h"

#include <algorithm>
#include <cctype>

#include "PlusFeatures.h"

namespace chewing_plus {
namespace {

std::string Str(char *s) {
    std::string r = s ? s : "";
    if (s) {
        chewing_free(s);
    }
    return r;
}
std::string Buffer(ChewingContext *ctx) {
    return chewing_buffer_Check(ctx) ? Str(chewing_buffer_String(ctx)) : "";
}
std::string Zuin(ChewingContext *ctx) {
    return chewing_bopomofo_Check(ctx) ? Str(chewing_bopomofo_String(ctx)) : "";
}
size_t CodePoints(const std::string &s) {
    return std::count_if(s.begin(), s.end(),
                         [](unsigned char c) { return (c & 0xC0) != 0x80; });
}
// Byte offset where the n-th code point starts (or size() past the end).
size_t ByteOffset(const std::string &s, size_t n) {
    size_t i = 0;
    while (i < s.size() && n) {
        ++i;
        while (i < s.size() &&
               (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) {
            ++i;
        }
        --n;
    }
    return i;
}
bool CodeChar(char c) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
           c == '+' || c == '-';
}
// Keys that are Bopomofo/tone/symbol-menu keys on some Chewing layout, so a
// single-key prefix on them would break normal typing.
bool UnsafeSingleKey(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) ||
           std::string(";,./-'`=[]\\").find(c) != std::string::npos;
}

} // namespace

std::pair<std::string, std::string>
ChewingPlus::splitBuffer(ChewingContext *ctx) {
    std::string buffer = Buffer(ctx);
    int cursor = chewing_cursor_Current(ctx);
    size_t at = buffer.size();
    if (cursor >= 0 && static_cast<size_t>(cursor) < CodePoints(buffer)) {
        at = ByteOffset(buffer, cursor);
    }
    return {buffer.substr(0, at), buffer.substr(at)};
}

void ChewingPlus::commitAround(ChewingContext *ctx, const std::string &middle,
                               const Commit &commit) {
    auto [head, tail] = splitBuffer(ctx);
    std::string text = head + middle + tail;
    chewing_cand_close(ctx);
    chewing_clean_bopomofo_buf(ctx);
    chewing_clean_preedit_buf(ctx);
    closePanel();
    if (!text.empty()) {
        commit(text);
    }
}

void ChewingPlus::openPanel(ChewingContext *ctx, Panel::Kind kind,
                            std::string input) {
    panel_ = {};
    panel_.kind = kind;
    panel_.input = std::move(input);
    refreshPanel(ctx);
}

void ChewingPlus::refreshPanel(ChewingContext *ctx) {
    auto [head, tail] = splitBuffer(ctx);
    panel_.labels.clear();
    panel_.values.clear();
    std::string marker;
    switch (panel_.kind) {
    case Panel::Kind::Calculator:
        marker = "[計算器] =";
        if (auto r = Calculate(panel_.input)) {
            panel_.labels.push_back(*r);
            panel_.values.push_back(*r);
        }
        break;
    case Panel::Kind::EmojiShortcode:
        marker = std::string(1, options_.emojiPrefix.key);
        if (options_.emojiPrefix.doubleTap) {
            marker += marker;
        }
        for (const auto &m : shortcodes_->search(panel_.input)) {
            panel_.labels.push_back(m.emoji + " :" + m.code);
            panel_.values.push_back(m.emoji);
        }
        break;
    case Panel::Kind::English:
        marker = "[EN] ";
        if (!panel_.input.empty()) {
            for (const auto &w : lexicon_->candidates(panel_.input)) {
                panel_.labels.push_back(w);
                panel_.values.push_back(w);
            }
        }
        break;
    case Panel::Kind::None:
        return;
    }
    panel_.preedit = head + marker + panel_.input;
    panel_.cursor = panel_.preedit.size();
    panel_.preedit += tail;
}

// English text is ASCII, so it can go into the Chewing buffer itself via
// SYMBOL_MODE: the Chinese around it stays editable and nothing is committed
// early. When the buffer is empty Chewing passes the characters straight
// through as commits, which are forwarded.
static void InsertAscii(ChewingContext *ctx, const std::string &text,
                        const ChewingPlus::Commit &commit) {
    int mode = chewing_get_ChiEngMode(ctx);
    chewing_set_ChiEngMode(ctx, SYMBOL_MODE);
    std::string out;
    for (char c : text) {
        chewing_handle_Default(ctx, c);
        if (chewing_commit_Check(ctx)) {
            out += Str(chewing_commit_String(ctx));
        }
    }
    chewing_set_ChiEngMode(ctx, mode);
    if (!out.empty()) {
        commit(out);
    }
}

bool ChewingPlus::toggleEnglish(ChewingContext *ctx, const Commit &commit) {
    if (panel_.kind == Panel::Kind::English) {
        std::string word = lexicon_->canonical(panel_.input);
        closePanel();
        InsertAscii(ctx, word, commit);
        return true;
    }
    if (!options_.english || panel_.active() ||
        chewing_get_ChiEngMode(ctx) != CHINESE_MODE ||
        chewing_cand_TotalChoice(ctx) > 0) {
        return false;
    }
    if (options_.englishHintCommit && !englishHint(ctx).empty()) {
        // The reading spells a known word (the hint is showing): put that
        // word in place of the unfinished syllable, keeping the rest of the
        // buffer, as Space would after typing it in the [EN] panel.
        std::string word = lexicon_->canonical(keys_);
        chewing_clean_bopomofo_buf(ctx);
        keys_.clear();
        overwrites_ = 0;
        overwriteFlags_.clear();
        lastZuinLen_ = 0;
        InsertAscii(ctx, word + " ", commit);
        return true;
    }
    std::string keys;
    if (!Zuin(ctx).empty()) {
        // Convert the unfinished reading only if it was typed as plain keys we
        // tracked; otherwise never discard a half-typed syllable.
        if (keys_.empty()) {
            return false;
        }
        keys = keys_;
        chewing_clean_bopomofo_buf(ctx);
        keys_.clear();
        overwrites_ = 0;
        overwriteFlags_.clear();
        lastZuinLen_ = 0;
    }
    openPanel(ctx, Panel::Kind::English, keys);
    return true;
}

Handled ChewingPlus::onKey(ChewingContext *ctx, char ascii, bool ctrl,
                           bool isBackspace, bool isEscape, bool isReturn,
                           bool isSpace, const Commit &commit) {
    if (panel_.active()) {
        auto &in = panel_.input;
        if (isEscape) {
            closePanel();
            return Handled::Yes;
        }
        if (panel_.kind == Panel::Kind::English) {
            if (isReturn) {
                std::string word = in;
                closePanel();
                InsertAscii(ctx, word, commit);
            } else if (isSpace) {
                std::string word = lexicon_->canonical(in);
                closePanel();
                InsertAscii(ctx, word.empty() ? word : word + " ", commit);
            } else if (isBackspace) {
                if (in.empty()) {
                    closePanel();
                } else {
                    in.pop_back();
                    refreshPanel(ctx);
                }
            } else if (ascii > 0x20 && ascii < 0x7f && !ctrl) {
                if (in.size() < 40) {
                    in += ascii;
                }
                refreshPanel(ctx);
            }
            return Handled::Yes; // modal
        }
        if (isReturn ||
            (isSpace && panel_.kind == Panel::Kind::EmojiShortcode)) {
            if (!panel_.values.empty()) {
                select(ctx, 0, commit);
            }
            return Handled::Yes;
        }
        if (isBackspace) {
            if (in.empty()) {
                closePanel();
            } else {
                in.pop_back();
                refreshPanel(ctx);
            }
            return Handled::Yes;
        }
        if (panel_.kind == Panel::Kind::Calculator && ascii &&
            std::string("0123456789.+-*/%() ").find(ascii) !=
                std::string::npos &&
            in.size() < 256) {
            in += ascii;
            refreshPanel(ctx);
        } else if (panel_.kind == Panel::Kind::EmojiShortcode &&
                   CodeChar(ascii) && in.size() < 40) {
            in += static_cast<char>(
                std::tolower(static_cast<unsigned char>(ascii)));
            refreshPanel(ctx);
        }
        return Handled::Yes;
    }

    if (chewing_get_ChiEngMode(ctx) != CHINESE_MODE || ctrl) {
        prefixPendingMs_ = -1;
        return Handled::No;
    }
    const bool empty = Buffer(ctx).empty() && Zuin(ctx).empty();

    if (options_.calculator && ascii == '=' && empty) {
        openPanel(ctx, Panel::Kind::Calculator, "");
        return Handled::Yes;
    }

    const auto &prefix = options_.emojiPrefix;
    const int64_t pending = prefixPendingMs_;
    prefixPendingMs_ = -1;
    if (options_.emojiShortcode && prefix.valid() && ascii == prefix.key) {
        if (!prefix.doubleTap) {
            if (!UnsafeSingleKey(ascii) && empty) {
                openPanel(ctx, Panel::Kind::EmojiShortcode, "");
                return Handled::Yes;
            }
            return Handled::No;
        }
        if (pending >= 0 && nowMs_ - pending <= options_.emojiDoubleTapMs) {
            // Undo exactly what the first tap produced: either a one-key
            // reading (e.g. ㄤ on the default layout) or one character
            // appended at the end of the buffer.
            const std::string zuin = Zuin(ctx);
            const std::string buffer = Buffer(ctx);
            bool undo = false;
            if (prefixPendingZuin_.empty() && !zuin.empty() &&
                CodePoints(zuin) == 1 && buffer == prefixPendingBuffer_) {
                chewing_clean_bopomofo_buf(ctx);
                undo = true;
            } else if (prefixPendingZuin_.empty() && zuin.empty() &&
                       CodePoints(buffer) ==
                           CodePoints(prefixPendingBuffer_) + 1 &&
                       buffer.starts_with(prefixPendingBuffer_) &&
                       static_cast<size_t>(chewing_cursor_Current(ctx)) ==
                           CodePoints(buffer)) {
                chewing_handle_Backspace(ctx);
                undo = Buffer(ctx) == prefixPendingBuffer_;
            }
            if (undo) {
                keys_.clear();
                overwrites_ = 0;
                overwriteFlags_.clear();
                lastZuinLen_ = 0;
                openPanel(ctx, Panel::Kind::EmojiShortcode, "");
                return Handled::Yes;
            }
        }
        if (Zuin(ctx).empty()) { // Only a fresh tap can be a first tap.
            prefixPendingMs_ = nowMs_;
            prefixPendingBuffer_ = Buffer(ctx);
            prefixPendingZuin_.clear();
        }
    }
    return Handled::No;
}

void ChewingPlus::insertDictation(ChewingContext *ctx, const std::string &text,
                                  const Commit &commit) {
    if (text.empty()) {
        return;
    }
    closePanel();
    chewing_cand_close(ctx);
    // A half-typed syllable is not part of the text: drop it rather than
    // committing raw Bopomofo.
    chewing_clean_bopomofo_buf(ctx);
    keys_.clear();
    overwrites_ = 0;
    overwriteFlags_.clear();
    lastZuinLen_ = 0;
    const bool ascii = std::all_of(
        text.begin(), text.end(), [](char c) { return c >= 0x20 && c < 0x7f; });
    if (ascii) {
        InsertAscii(ctx, text, commit);
    } else {
        commitAround(ctx, text, commit);
    }
}

void ChewingPlus::select(ChewingContext *ctx, int i, const Commit &commit) {
    if (!panel_.active() || i < 0 ||
        static_cast<size_t>(i) >= panel_.values.size()) {
        return;
    }
    std::string value = panel_.values[i];
    switch (panel_.kind) {
    case Panel::Kind::English:
        closePanel();
        InsertAscii(ctx, value, commit);
        break;
    case Panel::Kind::Calculator:
        closePanel();
        commit(value); // Only opened on an empty buffer.
        break;
    case Panel::Kind::EmojiShortcode:
        // Not ASCII: Chewing's buffer cannot hold it, so commit the
        // composition with the emoji at the cursor.
        commitAround(ctx, value, commit);
        break;
    case Panel::Kind::None:
        break;
    }
}

std::optional<std::pair<std::string, std::string>>
ChewingPlus::emojiForCandidates(ChewingContext *ctx) const {
    if (!options_.emoji || chewing_cand_CurrentPage(ctx) != 0) {
        return std::nullopt;
    }
    std::string buffer = Buffer(ctx);
    if (buffer.empty() || !Zuin(ctx).empty() ||
        static_cast<size_t>(chewing_cursor_Current(ctx)) < CodePoints(buffer)) {
        return std::nullopt;
    }
    auto emoji = EmojiForText(buffer);
    if (emoji.empty()) {
        return std::nullopt;
    }
    // Shown and committed as the whole composition plus the emoji.
    return std::make_pair(emoji.front(), emoji.front());
}

void ChewingPlus::afterChewingKey(ChewingContext *ctx, char ascii,
                                  bool isBackspace) {
    const std::string zuin = Zuin(ctx);
    if (zuin.empty()) {
        keys_.clear();
        overwrites_ = 0;
        overwriteFlags_.clear();
        lastZuinLen_ = 0;
        initials_.clear();
        return;
    }
    size_t len = CodePoints(zuin);
    // ㄅ..ㄙ (U+3105..U+3119): E3 84 85..99. A lone initial that replaced
    // another lone initial extends the abbreviation.
    const bool loneInitial = zuin.size() == 3 &&
                             static_cast<unsigned char>(zuin[0]) == 0xE3 &&
                             static_cast<unsigned char>(zuin[1]) == 0x84 &&
                             static_cast<unsigned char>(zuin[2]) >= 0x85 &&
                             static_cast<unsigned char>(zuin[2]) <= 0x99;
    if (!loneInitial) {
        initials_.clear();
    } else if (isBackspace) {
        if (initials_.size() >= 3) {
            initials_.resize(initials_.size() - 3);
        }
        if (initials_.empty()) {
            initials_ = zuin;
        }
    } else if (lastZuinLen_ == 1 && !initials_.empty()) {
        initials_ += zuin;
    } else {
        initials_ = zuin;
    }
    if (isBackspace) {
        if (!keys_.empty()) {
            keys_.pop_back();
            if (!overwriteFlags_.empty()) {
                overwrites_ -= overwriteFlags_.back();
                overwriteFlags_.pop_back();
            }
        }
    } else if (ascii > 0x20 && ascii < 0x7f && keys_.size() < 40) {
        keys_ += ascii;
        bool overwrote = len <= lastZuinLen_;
        overwriteFlags_.push_back(overwrote);
        overwrites_ += overwrote;
    }
    lastZuinLen_ = len;
}

std::string ChewingPlus::englishHint(ChewingContext *ctx) const {
    if (!options_.english || !options_.englishHint || Zuin(ctx).empty()) {
        return {};
    }
    if (keys_.size() < 4 || overwrites_ < 2) {
        return {};
    }
    if (!std::all_of(keys_.begin(), keys_.end(),
                     [](char c) { return c >= 'a' && c <= 'z'; })) {
        return {};
    }
    if (!lexicon_->contains(keys_)) {
        return {};
    }
    return options_.englishShortcutLabel + " → " + lexicon_->canonical(keys_);
}

std::string ChewingPlus::postProcessCommit(const std::string &text) const {
    if (!options_.spaceLetters && !options_.spaceDigits) {
        return text;
    }
    return AutoSpace(text, options_.spaceLetters, options_.spaceDigits);
}

} // namespace chewing_plus
