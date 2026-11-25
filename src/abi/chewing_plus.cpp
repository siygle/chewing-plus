/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "chewing_plus.h"

#include <chewing.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ChewingPlus.h"
#include "PlusData.h"
#include "PlusPhrases.h"

namespace {

constexpr int kMaxChiSymbolLen = 18;

size_t CodePoints(const std::string &s) {
    return static_cast<size_t>(
        std::count_if(s.begin(), s.end(),
                      [](unsigned char c) { return (c & 0xC0) != 0x80; }));
}

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

std::string BufferOf(ChewingContext *ctx) {
    if (!chewing_buffer_Check(ctx)) {
        return {};
    }
    const char *s = chewing_buffer_String_static(ctx);
    return s ? s : "";
}

std::string ZuinOf(ChewingContext *ctx) {
    if (!chewing_bopomofo_Check(ctx)) {
        return {};
    }
    const char *s = chewing_bopomofo_String_static(ctx);
    return s ? s : "";
}

std::string AuxOf(ChewingContext *ctx) {
    if (!chewing_aux_Check(ctx)) {
        return {};
    }
    const char *s = chewing_aux_String_static(ctx);
    return s ? s : "";
}

std::string CommitOf(ChewingContext *ctx) {
    if (!chewing_commit_Check(ctx)) {
        return {};
    }
    const char *s = chewing_commit_String_static(ctx);
    return s ? s : "";
}

struct Candidate {
    std::string text;
    std::string label;
    int kind = CHEWING_PLUS_CAND_CHEWING;
    int index = 0;
};

struct Session {
    struct Delete {
        void operator()(ChewingContext *ctx) const {
            if (ctx) {
                chewing_delete(ctx);
            }
        }
    };

    std::unique_ptr<ChewingContext, Delete> ctx;
    chewing_plus::ChewingPlus plus;
    chewing_plus::DoubleTap englishTap;
    chewing_plus::Options opt;
    bool sensitive = false;
    bool englishDoubleTap = false;
    bool englishCombo = true;
    bool shiftToggle = false;
    bool asciiMode = false;
    // Lone Left Shift: pressed with nothing else since.
    bool shiftPending = false;
    int64_t shiftDownMs = 0;
    bool easySymbol = false;
    std::string layout = "KB_DEFAULT";
    std::string selKeys = "1234567890";
    std::string emojiBuiltin;
    std::string emojiUser;
    std::string englishBuiltin;
    std::string englishUser;
    std::string phrasesPath;
    bool suggestions = false;
    // Initials typed before suggesting phrases (ㄇㄇ -> 媽媽 at 2).
    size_t initialsMin = 2;
    // Likely characters for the syllable being typed (live_chars).
    bool liveChars = false;
    std::string charsPath;
    std::shared_ptr<const chewing_plus::PhraseIndex> chars =
        std::make_shared<chewing_plus::PhraseIndex>();
    // The list holds characters for the reading, not phrases.
    bool suggChars = false;
    std::shared_ptr<const chewing_plus::PhraseIndex> phrases =
        std::make_shared<chewing_plus::PhraseIndex>();
    // Current suggestions and what they replace: the last `replaceChars`
    // code points before the cursor in the buffer, or the whole reading.
    std::vector<std::string> sugg;
    size_t replaceChars = 0;
    bool replaceReading = false;
    chewing_plus_callbacks cb{};
    chewing_plus_preceding_text_fn precedingFn = nullptr;
    void *precedingUser = nullptr;

    // Application text before the cursor, "" when unknown or sensitive.
    std::string applicationPrecedingText() {
        if (sensitive || !precedingFn) {
            return {};
        }
        char buf[64];
        buf[0] = '\0';
        int n = precedingFn(precedingUser, buf, sizeof(buf));
        if (n <= 0) {
            return {};
        }
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    }

    std::string preedit;
    size_t cursor = 0;
    size_t hlStart = 0;
    size_t hlLen = 0;
    std::string aux;
    std::string panelInput;
    int panelKind = CHEWING_PLUS_PANEL_NONE;
    std::vector<Candidate> cands;
    std::string pendingCommit;
    std::string takenCommit;
    // Current value of every setting, by key (settings.inc).
    std::map<std::string, std::string> settings;
    std::string settingOut;
    // Bumped by refresh() when anything a frontend draws changed.
    uint64_t revision = 1;
    std::string lastDrawn;

    ChewingContext *raw() { return ctx.get(); }

    void commitText(const std::string &text) {
        if (text.empty()) {
            return;
        }
        std::string out = sensitive ? text : plus.postProcessCommit(text);
        pendingCommit += out;
        if (cb.commit) {
            cb.commit(cb.user, out.c_str());
        }
    }

    void reloadData() {
        // Shared with every other session using the same files.
        plus.setShortcodes(opt.emojiShortcode ? chewing_plus::DataCache::emoji(
                                                    emojiBuiltin, emojiUser)
                                              : nullptr);
        plus.setLexicon(opt.english ? chewing_plus::DataCache::english(
                                          englishBuiltin, englishUser)
                                    : nullptr);
    }

    void applyOptions() { plus.setOptions(opt); }

    void applyEngine() {
        ChewingContext *c = raw();
        chewing_set_maxChiSymbolLen(c, kMaxChiSymbolLen);
        chewing_set_KBType(c, chewing_KBStr2Num(layout.c_str()));
        chewing_set_ChiEngMode(c, CHINESE_MODE);
        if (selKeys.size() >= 10) {
            int sel[10];
            for (int i = 0; i < 10; i++) {
                sel[i] = static_cast<unsigned char>(selKeys[i]);
            }
            chewing_set_selKey(c, sel, 10);
        }
        chewing_set_escCleanAllBuf(c, 1);
    }

    void refresh() {
        ChewingContext *c = raw();
        const auto &panel = plus.panel();
        cands.clear();
        hlStart = 0;
        hlLen = 0;
        if (panel.active()) {
            preedit = panel.preedit;
            cursor = panel.cursor;
            panelInput = panel.input;
            switch (panel.kind) {
            case chewing_plus::Panel::Kind::Calculator:
                panelKind = CHEWING_PLUS_PANEL_CALCULATOR;
                break;
            case chewing_plus::Panel::Kind::EmojiShortcode:
                panelKind = CHEWING_PLUS_PANEL_EMOJI;
                break;
            case chewing_plus::Panel::Kind::English:
                panelKind = CHEWING_PLUS_PANEL_ENGLISH;
                break;
            case chewing_plus::Panel::Kind::None:
                panelKind = CHEWING_PLUS_PANEL_NONE;
                break;
            }
            const bool english =
                panel.kind == chewing_plus::Panel::Kind::English;
            for (size_t i = 0; i < panel.labels.size() && i < 9; i++) {
                Candidate cand;
                cand.text = panel.labels[i];
                cand.kind = CHEWING_PLUS_CAND_PANEL;
                cand.index = static_cast<int>(i);
                cand.label = std::string(english ? "^" : "⇧") +
                             std::to_string(i + 1) + ". ";
                cands.push_back(std::move(cand));
            }
            aux.clear();
        } else {
            panelKind = CHEWING_PLUS_PANEL_NONE;
            panelInput.clear();
            std::string buffer = BufferOf(c);
            std::string zuin = ZuinOf(c);
            size_t len = CodePoints(buffer);
            int cur = chewing_cursor_Current(c);
            size_t at = buffer.size();
            if (cur >= 0 && static_cast<size_t>(cur) < len) {
                at = ByteOffset(buffer, static_cast<size_t>(cur));
            }
            preedit = buffer.substr(0, at) + zuin + buffer.substr(at);
            cursor = at;
            if (!zuin.empty()) {
                hlStart = at;
                hlLen = zuin.size();
            }
            aux = AuxOf(c);
            if (aux.empty() && !sensitive) {
                aux = plus.englishHint(c);
            }
            fillChewingCandidates(c);
        }
        fillSuggestions(c);
        bumpRevision();
        if (cb.updated) {
            cb.updated(cb.user);
        }
    }

    // One string that changes whenever anything visible changes.
    void bumpRevision() {
        std::string key;
        key.reserve(preedit.size() + aux.size() + 64);
        auto add = [&key](const std::string &s) {
            key += s;
            key += '\x1f';
        };
        add(preedit);
        add(std::to_string(cursor) + "," + std::to_string(hlStart) + "," +
            std::to_string(hlLen) + "," + std::to_string(panelKind));
        add(aux);
        add(asciiMode ? "A" : "C");
        for (const auto &c : cands) {
            add(c.label + c.text);
        }
        key += '\x1e';
        for (const auto &s : sugg) {
            add(s);
        }
        if (key != lastDrawn) {
            lastDrawn = std::move(key);
            ++revision;
        }
    }

    // Longest phrase prefix ending at the cursor that has completions, or
    // the reading expanded as initials. Nothing while a candidate list or
    // panel is open, or in a password field.
    void fillSuggestions(ChewingContext *c) {
        sugg.clear();
        replaceChars = 0;
        replaceReading = false;
        suggChars = false;
        if (sensitive || panelKind != CHEWING_PLUS_PANEL_NONE ||
            chewing_cand_TotalPage(c) > 0) {
            return;
        }
        std::string buffer = BufferOf(c);
        std::string zuin = ZuinOf(c);
        if (!suggestions || phrases->size() == 0) {
            fillLiveChars(zuin);
            return;
        }
        if (!zuin.empty()) {
            // Initials only: each syllable typed as its first symbol, so the
            // reading holds one symbol per key (libchewing overwrites the
            // initial slot, which the frontends see as a growing reading
            // only on layouts that queue them, e.g. Hsu). Use the raw keys
            // recorded for the English hint when the reading does not grow.
            std::string initials = plus.typedInitials(c);
            sugg = phrases->expand(initials, 9, initialsMin);
            replaceReading = !sugg.empty();
            if (!replaceReading) {
                fillLiveChars(zuin);
                return;
            }
            if (replaceReading && initials != zuin) {
                // Show the text before the initials, then all of them.
                // libchewing shows only the last one, and with toneless
                // typing it has turned the others into characters.
                size_t start = plus.initialsStart();
                size_t keep = std::min(start, CodePoints(buffer));
                preedit = buffer.substr(0, ByteOffset(buffer, keep)) + initials;
                hlStart = ByteOffset(buffer, keep);
                hlLen = initials.size();
                cursor = preedit.size();
            }
            return;
        }
        size_t len = CodePoints(buffer);
        int cur = chewing_cursor_Current(c);
        size_t end = (cur >= 0 && static_cast<size_t>(cur) < len)
                         ? static_cast<size_t>(cur)
                         : len;
        // Try the longest tail first (up to 6 characters) so 中華 wins over 華.
        for (size_t n = std::min<size_t>(end, 6); n >= 2; --n) {
            size_t from = ByteOffset(buffer, end - n);
            std::string tail =
                buffer.substr(from, ByteOffset(buffer, end) - from);
            auto found = phrases->complete(tail);
            if (!found.empty()) {
                sugg = std::move(found);
                replaceChars = n;
                return;
            }
        }
    }

    // ㄋ -> 你 年 您: characters whose toneless reading starts with what is
    // typed so far. Tones in the reading are dropped before the lookup.
    void fillLiveChars(const std::string &zuin) {
        if (!liveChars || zuin.empty() || chars->size() == 0) {
            return;
        }
        std::string reading;
        for (size_t i = 0; i + 1 < zuin.size() || i < zuin.size();) {
            auto b = static_cast<unsigned char>(zuin[i]);
            size_t n = b < 0x80 ? 1 : b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4;
            std::string cp = zuin.substr(i, n);
            if (chewing_plus::AllBopomofo(cp)) {
                reading += cp;
            }
            i += n;
        }
        sugg = chars->byReading(reading);
        replaceReading = !sugg.empty();
        suggChars = replaceReading;
    }

    void selectSuggestion(ChewingContext *c, int index) {
        if (index < 0 || index >= static_cast<int>(sugg.size())) {
            return;
        }
        std::string pick = sugg[index];
        if (suggChars) {
            // A character for the syllable being typed: it replaces the
            // reading. libchewing's buffer cannot hold an arbitrary
            // character, so the composition is committed with it at the
            // cursor (the same path as dictation and emoji).
            insertText(c, pick);
            return;
        }
        if (replaceReading) {
            chewing_plus::ChewingPlus::dropAfter(c, plus.initialsStart());
            plus.reset();
        } else {
            for (size_t i = 0; i < replaceChars; ++i) {
                chewing_handle_Backspace(c);
            }
        }
        insertText(c, pick);
    }

    // Same path as dictation: ASCII stays in the buffer, anything else is
    // committed with the composition around it.
    void insertText(ChewingContext *c, const std::string &text) {
        plus.insertDictation(c, text,
                             [this](const std::string &t) { commitText(t); });
    }

    void fillChewingCandidates(ChewingContext *c) {
        int pageSize = chewing_cand_ChoicePerPage(c);
        if (pageSize <= 0) {
            return;
        }
        // fcitx still asks Plus for an emoji row when libchewing's own
        // candidate window is closed (TotalPage == 0). The row is the list.
        if (chewing_cand_TotalPage(c) > 0) {
            chewing_cand_Enumerate(c);
        }
        int index = 0;
        while (chewing_cand_TotalPage(c) > 0 && chewing_cand_hasNext(c) &&
               index < pageSize) {
            const char *s = chewing_cand_String_static(c);
            Candidate cand;
            cand.text = s ? s : "";
            cand.kind = CHEWING_PLUS_CAND_CHEWING;
            cand.index = index;
            if (index < 10 && index < static_cast<int>(selKeys.size())) {
                cand.label = std::string(1, selKeys[index]) + ". ";
            }
            cands.push_back(std::move(cand));
            index++;
        }
        if (sensitive) {
            return;
        }
        if (auto extra = plus.emojiForCandidates(c)) {
            Candidate cand;
            cand.text = extra->first;
            cand.kind = CHEWING_PLUS_CAND_EMOJI;
            cand.index = -1;
            cand.label = "☺ ";
            auto pos = std::min<size_t>(2, cands.size());
            cands.insert(cands.begin() + static_cast<long>(pos),
                         std::move(cand));
        }
    }

    void setAscii(bool on) {
        if (on == asciiMode) {
            return;
        }
        if (on) {
            flush(CHEWING_PLUS_FLUSH_DEFAULT);
        }
        asciiMode = on;
        shiftPending = false;
        refresh();
    }

    // Commit as asked, then leave everything empty.
    void flush(int mode) {
        ChewingContext *c = raw();
        std::string text;
        if (plus.panel().active()) {
            // A Plus panel holds typed input, not a composition.
            if (mode == CHEWING_PLUS_FLUSH_PREEDIT) {
                text = BufferOf(c);
            }
        } else if (mode != CHEWING_PLUS_FLUSH_CLEAR) {
            chewing_cand_close(c);
            if (chewing_buffer_Check(c) && chewing_commit_preedit_buf(c) == 0) {
                text = CommitOf(c);
            }
            if (mode == CHEWING_PLUS_FLUSH_PREEDIT) {
                text += BufferOf(c) + ZuinOf(c);
            }
        }
        plus.reset();
        englishTap.reset();
        chewing_cand_close(c);
        chewing_clean_preedit_buf(c);
        chewing_clean_bopomofo_buf(c);
        chewing_Reset(c);
        if (!text.empty()) {
            commitText(text);
        }
        refresh();
    }

    // Wrap from the last page to the first and back, like fcitx5-chewing.
    bool page(bool next) {
        ChewingContext *c = raw();
        if (cands.empty() || chewing_cand_TotalPage(c) <= 0) {
            return false;
        }
        const int current = chewing_cand_CurrentPage(c);
        if (next) {
            if (current == chewing_cand_TotalPage(c) - 1) {
                chewing_handle_Down(c);
            } else {
                chewing_handle_PageDown(c);
            }
        } else if (current == 0 && (chewing_cand_list_has_next(c) == 1 ||
                                    chewing_cand_list_has_prev(c) == 1)) {
            chewing_handle_Down(c);
        } else {
            chewing_handle_PageUp(c);
        }
        const bool changed = chewing_keystroke_CheckAbsorb(c) != 0;
        refresh();
        return changed;
    }

    bool toggleEnglish() {
        if (sensitive) {
            return false;
        }
        return plus.toggleEnglish(
            raw(), [this](const std::string &t) { commitText(t); });
    }

    void selectChewing(ChewingContext *c, int index) {
        int pageSize = chewing_get_candPerPage(c);
        if (pageSize <= 0) {
            return;
        }
        int page = (index / pageSize) + chewing_cand_CurrentPage(c);
        int off = index % pageSize;
        if (page < 0 || page >= chewing_cand_TotalPage(c)) {
            return;
        }
        if (off < 0 || off >= static_cast<int>(selKeys.size())) {
            return;
        }
        int lastPage = chewing_cand_CurrentPage(c);
        while (page != chewing_cand_CurrentPage(c)) {
            if (page < chewing_cand_CurrentPage(c)) {
                chewing_handle_Left(c);
            }
            if (page > chewing_cand_CurrentPage(c)) {
                chewing_handle_Right(c);
            }
            if (lastPage == chewing_cand_CurrentPage(c)) {
                break;
            }
            lastPage = chewing_cand_CurrentPage(c);
        }
        chewing_handle_Default(c, static_cast<unsigned char>(selKeys[off]));
        if (chewing_keystroke_CheckIgnore(c)) {
            return;
        }
        if (auto text = CommitOf(c); !text.empty()) {
            commitText(text);
        }
    }

    void selectEmoji(ChewingContext *c, const std::string &text) {
        chewing_cand_close(c);
        chewing_clean_bopomofo_buf(c);
        chewing_clean_preedit_buf(c);
        commitText(text);
    }

    int handle(const chewing_plus_key_event &ev) {
        ChewingContext *c = raw();
        const bool release = (ev.mods & CHEWING_PLUS_MOD_RELEASE) != 0;
        const bool ctrl = (ev.mods & CHEWING_PLUS_MOD_CTRL) != 0;
        const bool shift = (ev.mods & CHEWING_PLUS_MOD_SHIFT) != 0;
        const bool alt = (ev.mods & CHEWING_PLUS_MOD_ALT) != 0;
        const int64_t now =
            ev.time_ms >= 0
                ? ev.time_ms
                : std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();
        plus.setNowMs(now);

        if (shiftToggle && !sensitive) {
            if (ev.key == CHEWING_PLUS_KEY_SHIFT_L) {
                if (!release) {
                    shiftPending = true;
                    shiftDownMs = now;
                } else if (shiftPending && now - shiftDownMs <= 500) {
                    shiftPending = false;
                    setAscii(!asciiMode);
                    return 1;
                }
            } else {
                shiftPending = false;
            }
        }
        if (asciiMode) {
            return 0;
        }

        if (opt.english && englishDoubleTap) {
            const bool watched = ev.key == CHEWING_PLUS_KEY_SHIFT_R;
            if (englishTap.onKey(watched, release, now) && !sensitive &&
                toggleEnglish()) {
                refresh();
                return 1;
            }
        }
        if (release) {
            return 0;
        }

        const bool isBackspace = ev.key == CHEWING_PLUS_KEY_BACKSPACE;
        const bool isEscape = ev.key == CHEWING_PLUS_KEY_ESCAPE;
        const bool isReturn = ev.key == CHEWING_PLUS_KEY_RETURN;
        const bool isSpace =
            ev.key == CHEWING_PLUS_KEY_SPACE || ev.ascii == ' ';
        const uint32_t ascii = ev.ascii;

        if (!sensitive) {
            if (opt.english && englishCombo && isSpace && ctrl && shift &&
                !alt && toggleEnglish()) {
                refresh();
                return 1;
            }
            if (plus.panel().active()) {
                const bool english =
                    plus.panel().kind == chewing_plus::Panel::Kind::English;
                const bool selMod = english ? ctrl : shift;
                int digit = -1;
                if (ev.keycode >= 10 && ev.keycode <= 18) {
                    digit = ev.keycode - 10;
                } else if (ascii >= '1' && ascii <= '9') {
                    digit = static_cast<int>(ascii - '1');
                }
                if (selMod && digit >= 0) {
                    plus.select(c, digit, [this](const std::string &t) {
                        commitText(t);
                    });
                    refresh();
                    return 1;
                }
            }
            char plusAscii = 0;
            if (!ctrl && ascii >= 0x20 && ascii < 0x7f) {
                plusAscii = static_cast<char>(ascii);
            }
            auto handled = plus.onKey(
                c, ctrl ? 0 : plusAscii, ctrl, isBackspace, isEscape, isReturn,
                isSpace, [this](const std::string &t) { commitText(t); });
            if (handled == chewing_plus::Handled::Yes) {
                refresh();
                return 1;
            }
        }

        if (plus.panel().active()) {
            refresh();
            return 1;
        }

        // With only a Plus emoji row showing, fcitx gives Up/Down/Page to
        // that list instead of opening libchewing phrase selection (which
        // would clamp the cursor and hide the row). The highlight itself
        // stays in the frontend; select() picks the row.
        const bool navigation = ev.key == CHEWING_PLUS_KEY_UP ||
                                ev.key == CHEWING_PLUS_KEY_DOWN ||
                                ev.key == CHEWING_PLUS_KEY_LEFT ||
                                ev.key == CHEWING_PLUS_KEY_RIGHT ||
                                ev.key == CHEWING_PLUS_KEY_PAGE_UP ||
                                ev.key == CHEWING_PLUS_KEY_PAGE_DOWN;
        if (navigation && chewing_cand_TotalPage(c) <= 0) {
            bool plusOnly = !cands.empty();
            for (const auto &cand : cands) {
                if (cand.kind != CHEWING_PLUS_CAND_EMOJI) {
                    plusOnly = false;
                }
            }
            if (plusOnly) {
                refresh();
                return 1;
            }
        }

        const char trackAscii = (!ctrl && ascii > 0x20 && ascii < 0x7f)
                                    ? static_cast<char>(ascii)
                                    : 0;
        if (layout == "KB_HANYU_PINYIN" && trackAscii &&
            ZuinOf(c).size() >= 9) {
            refresh();
            return 1;
        }

        bool dispatched = true;
        if (isSpace && shift && !ctrl) {
            chewing_handle_ShiftSpace(c);
        } else if (isSpace) {
            chewing_handle_Space(c);
        } else if (ev.key == CHEWING_PLUS_KEY_TAB) {
            chewing_handle_Tab(c);
        } else if (ascii >= 0x20 && ascii < 0x7f && !ctrl &&
                   ev.key == CHEWING_PLUS_KEY_NONE) {
            if (shift && easySymbol) {
                chewing_set_easySymbolInput(c, 1);
            }
            chewing_handle_Default(c, static_cast<int>(ascii));
            chewing_set_easySymbolInput(c, 0);
        } else if (isBackspace) {
            if (!chewing_buffer_Check(c) && !chewing_bopomofo_Check(c)) {
                return 0;
            }
            chewing_handle_Backspace(c);
            if (!chewing_buffer_Check(c) && !chewing_bopomofo_Check(c)) {
                plus.reset();
                chewing_cand_close(c);
                chewing_clean_preedit_buf(c);
                chewing_clean_bopomofo_buf(c);
                chewing_Reset(c);
                refresh();
                return 1;
            }
        } else if (isEscape) {
            chewing_handle_Esc(c);
        } else if (ev.key == CHEWING_PLUS_KEY_DELETE) {
            if (!chewing_buffer_Check(c) && !chewing_bopomofo_Check(c)) {
                return 0;
            }
            chewing_handle_Del(c);
            if (!chewing_buffer_Check(c) && !chewing_bopomofo_Check(c)) {
                plus.reset();
                chewing_cand_close(c);
                chewing_clean_preedit_buf(c);
                chewing_clean_bopomofo_buf(c);
                chewing_Reset(c);
                refresh();
                return 1;
            }
        } else if (ev.key == CHEWING_PLUS_KEY_UP) {
            chewing_handle_Up(c);
        } else if (ev.key == CHEWING_PLUS_KEY_DOWN) {
            chewing_handle_Down(c);
        } else if (ev.key == CHEWING_PLUS_KEY_PAGE_DOWN) {
            chewing_handle_PageDown(c);
        } else if (ev.key == CHEWING_PLUS_KEY_PAGE_UP) {
            chewing_handle_PageUp(c);
        } else if (ev.key == CHEWING_PLUS_KEY_RIGHT && shift) {
            chewing_handle_ShiftRight(c);
        } else if (ev.key == CHEWING_PLUS_KEY_LEFT && shift) {
            chewing_handle_ShiftLeft(c);
        } else if (ev.key == CHEWING_PLUS_KEY_RIGHT) {
            chewing_handle_Right(c);
        } else if (ev.key == CHEWING_PLUS_KEY_LEFT) {
            chewing_handle_Left(c);
        } else if (ev.key == CHEWING_PLUS_KEY_HOME) {
            chewing_handle_Home(c);
        } else if (ev.key == CHEWING_PLUS_KEY_END) {
            chewing_handle_End(c);
        } else if (isReturn) {
            chewing_handle_Enter(c);
        } else if (ctrl && ascii >= '0' && ascii <= '9') {
            chewing_handle_CtrlNum(c, static_cast<int>(ascii));
        } else {
            dispatched = false;
        }

        if (!dispatched) {
            return 0;
        }
        if (chewing_keystroke_CheckIgnore(c)) {
            return 0;
        }
        int absorb = chewing_keystroke_CheckAbsorb(c);
        if (auto text = CommitOf(c); !text.empty()) {
            commitText(text);
            absorb = 1;
        }
        plus.afterChewingKey(c, trackAscii, isBackspace);
        refresh();
        return absorb ? 1 : 0;
    }
};

Session *S(chewing_plus_session *session) {
    return reinterpret_cast<Session *>(session);
}
const Session *S(const chewing_plus_session *session) {
    return reinterpret_cast<const Session *>(session);
}

} // namespace

namespace {

struct SettingRow {
    const char *key;
    int type;
    const char *defaultValue;
    const char *labelEn;
    const char *labelZh;
};

constexpr SettingRow kSettings[] = {
#define BOOL CHEWING_PLUS_SETTING_BOOL
#define INT CHEWING_PLUS_SETTING_INT
#define STRING CHEWING_PLUS_SETTING_STRING
#define SETTING(key, type, def, en, zh) {#key, type, def, en, zh},
#include "settings.inc"
#undef SETTING
#undef BOOL
#undef INT
#undef STRING
};

const SettingRow *FindSetting(const char *key) {
    if (!key) {
        return nullptr;
    }
    for (const auto &row : kSettings) {
        if (std::strcmp(row.key, key) == 0) {
            return &row;
        }
    }
    return nullptr;
}

// "1"/"0" for a boolean, or "" if it does not parse.
std::string ParseBool(const char *value) {
    std::string v = value ? value : "";
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (v == "1" || v == "true" || v == "on" || v == "yes") {
        return "1";
    }
    if (v == "0" || v == "false" || v == "off" || v == "no") {
        return "0";
    }
    return "";
}

bool ParseInt(const char *value, int &out) {
    if (!value || !*value) {
        return false;
    }
    char *end = nullptr;
    long n = std::strtol(value, &end, 10);
    if (*end != '\0' || n < -100000 || n > 100000) {
        return false;
    }
    out = static_cast<int>(n);
    return true;
}

// Push the stored values into the engine. Grouped settings (spacing,
// shortcode, English) are applied together from their stored keys.
void ApplySetting(chewing_plus_session *session, const std::string &key) {
    auto &st = S(session)->settings;
    auto on = [&st](const char *k) { return st[k] == "1" ? 1 : 0; };
    auto num = [&st](const char *k) {
        int n = 0;
        ParseInt(st[k].c_str(), n);
        return n;
    };
    if (key == "emoji") {
        chewing_plus_set_emoji(session, on("emoji"));
    } else if (key == "calculator") {
        chewing_plus_set_calculator(session, on("calculator"));
    } else if (key == "space_letters" || key == "space_digits") {
        chewing_plus_set_spacing(session, on("space_letters"),
                                 on("space_digits"));
    } else if (key == "shortcode" || key == "shortcode_prefix" ||
               key == "shortcode_tap_ms") {
        chewing_plus_set_emoji_shortcode(session, on("shortcode"),
                                         st["shortcode_prefix"].c_str(),
                                         num("shortcode_tap_ms"));
    } else if (key == "english" || key == "english_hint") {
        chewing_plus_set_english(session, on("english"), on("english_hint"),
                                 nullptr);
    } else if (key == "english_hint_commit") {
        chewing_plus_set_english_hint_commit(session,
                                             on("english_hint_commit"));
    } else if (key == "suggestions") {
        chewing_plus_set_suggestions(session, on("suggestions"), nullptr);
    } else if (key == "fuzzy") {
        chewing_config_set_int(S(session)->raw(), "chewing.conversion_engine",
                               on("fuzzy") ? FUZZY_CHEWING_CONVERSION_ENGINE
                                           : CHEWING_CONVERSION_ENGINE);
        S(session)->refresh();
    } else if (key == "live_chars") {
        auto *s = S(session);
        s->liveChars = on("live_chars");
        s->chars = s->liveChars && !s->charsPath.empty()
                       ? chewing_plus::DataCache::chars(s->charsPath)
                       : std::make_shared<chewing_plus::PhraseIndex>();
        s->refresh();
    } else if (key == "initials_min") {
        // Below 2 every lone initial would list thousands of phrases.
        S(session)->initialsMin =
            static_cast<size_t>(std::max(2, num("initials_min")));
        S(session)->refresh();
    } else if (key == "shift_toggle") {
        chewing_plus_set_shift_toggle(session, on("shift_toggle"));
    } else if (key == "easy_symbol") {
        chewing_plus_set_easy_symbol(session, on("easy_symbol"));
    } else if (key == "keyboard_layout") {
        chewing_plus_set_keyboard_layout(session,
                                         st["keyboard_layout"].c_str());
    } else if (key == "selection_keys") {
        chewing_plus_set_selection_keys(session, st["selection_keys"].c_str());
    } else if (key == "page_size") {
        chewing_plus_set_page_size(session, num("page_size"));
    } else if (key == "space_as_selection") {
        chewing_plus_set_space_as_selection(session, on("space_as_selection"));
    } else if (key == "add_phrase_forward" || key == "choice_backward" ||
               key == "auto_shift_cursor") {
        chewing_plus_set_phrase_options(session, on("add_phrase_forward"),
                                        on("choice_backward"),
                                        on("auto_shift_cursor"));
    }
}

} // namespace

extern "C" {

int chewing_plus_abi_version(void) { return CHEWING_PLUS_ABI_VERSION; }

const char *chewing_plus_libchewing_pin(void) {
    return CHEWING_PLUS_LIBCHEWING_PIN;
}

chewing_plus_session *chewing_plus_new(const char *chewing_sys_path,
                                       const char *chewing_user_path) {
    ChewingContext *raw =
        chewing_new2(chewing_sys_path, chewing_user_path, nullptr, nullptr);
    if (!raw) {
        return nullptr;
    }
    auto *session = new Session();
    session->ctx.reset(raw);
    session->opt.emojiPrefix = chewing_plus::EmojiPrefix::Parse(";;");
    session->opt.emojiDoubleTapMs = 300;
    session->englishTap.setWindowMs(300);
    session->plus.setPrecedingText(
        [session] { return session->applicationPrecedingText(); });
    session->applyOptions();
    session->applyEngine();
    chewing_set_candPerPage(raw, 10);
    chewing_set_addPhraseDirection(raw, 0);
    chewing_set_phraseChoiceRearward(raw, 1);
    chewing_set_autoShiftCur(raw, 0);
    chewing_set_spaceAsSelection(raw, 1);
    // Record the defaults; most match the engine state set above. The
    // ones the engine does not start with are applied.
    for (int i = 0; i < chewing_plus_setting_count(); i++) {
        chewing_plus_setting_info info;
        chewing_plus_setting_info_at(i, &info);
        session->settings[info.key] = info.default_value;
    }
    for (const char *key : {"fuzzy", "live_chars"}) {
        ApplySetting(reinterpret_cast<chewing_plus_session *>(session), key);
    }
    session->refresh();
    return reinterpret_cast<chewing_plus_session *>(session);
}

void chewing_plus_free(chewing_plus_session *session) { delete S(session); }

void chewing_plus_set_callbacks(chewing_plus_session *session,
                                const chewing_plus_callbacks *callbacks) {
    if (!session) {
        return;
    }
    S(session)->cb = callbacks ? *callbacks : chewing_plus_callbacks{};
}

void chewing_plus_set_preceding_text(chewing_plus_session *session,
                                     chewing_plus_preceding_text_fn fn,
                                     void *user) {
    if (!session) {
        return;
    }
    S(session)->precedingFn = fn;
    S(session)->precedingUser = fn ? user : nullptr;
}

void chewing_plus_set_data_paths(chewing_plus_session *session,
                                 const char *emoji_builtin,
                                 const char *emoji_user,
                                 const char *english_builtin,
                                 const char *english_user) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    if (emoji_builtin) {
        s->emojiBuiltin = emoji_builtin;
    }
    if (emoji_user) {
        s->emojiUser = emoji_user;
    }
    if (english_builtin) {
        s->englishBuiltin = english_builtin;
    }
    if (english_user) {
        s->englishUser = english_user;
    }
    s->reloadData();
}

void chewing_plus_set_suggestions(chewing_plus_session *session, int enabled,
                                  const char *path) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    bool reload = path && s->phrasesPath != path;
    if (path) {
        s->phrasesPath = path;
    }
    s->suggestions = enabled != 0;
    if (!s->suggestions || s->phrasesPath.empty()) {
        s->phrases = std::make_shared<chewing_plus::PhraseIndex>();
    } else if (reload || s->phrases->size() == 0) {
        s->phrases = chewing_plus::DataCache::phrases(s->phrasesPath);
    }
    s->refresh();
}

void chewing_plus_set_chars_path(chewing_plus_session *session,
                                 const char *path) {
    if (!session || !path) {
        return;
    }
    auto *s = S(session);
    s->charsPath = path;
    if (s->liveChars) {
        s->chars = chewing_plus::DataCache::chars(path);
        s->refresh();
    }
}

void chewing_plus_set_phrases_path(chewing_plus_session *session,
                                   const char *path) {
    if (!session || !path) {
        return;
    }
    auto *s = S(session);
    chewing_plus_set_suggestions(session, s->suggestions ? 1 : 0, path);
}

int chewing_plus_suggestion_count(const chewing_plus_session *session) {
    return session ? static_cast<int>(S(session)->sugg.size()) : 0;
}

const char *chewing_plus_suggestion(const chewing_plus_session *session,
                                    int index) {
    if (!session || index < 0 ||
        index >= static_cast<int>(S(session)->sugg.size())) {
        return nullptr;
    }
    return S(session)->sugg[index].c_str();
}

void chewing_plus_select_suggestion(chewing_plus_session *session, int index) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    s->selectSuggestion(s->raw(), index);
    s->refresh();
}

void chewing_plus_set_emoji(chewing_plus_session *session, int enabled) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    s->opt.emoji = enabled != 0;
    s->applyOptions();
}

void chewing_plus_set_calculator(chewing_plus_session *session, int enabled) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    s->opt.calculator = enabled != 0;
    s->applyOptions();
}

void chewing_plus_set_spacing(chewing_plus_session *session, int letters,
                              int digits) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    s->opt.spaceLetters = letters != 0;
    s->opt.spaceDigits = digits != 0;
    s->applyOptions();
}

void chewing_plus_set_emoji_shortcode(chewing_plus_session *session,
                                      int enabled, const char *prefix,
                                      int double_tap_ms) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    s->opt.emojiShortcode = enabled != 0;
    if (prefix) {
        s->opt.emojiPrefix = chewing_plus::EmojiPrefix::Parse(prefix);
    }
    if (double_tap_ms > 0) {
        s->opt.emojiDoubleTapMs = double_tap_ms;
    }
    s->applyOptions();
    s->reloadData();
}

void chewing_plus_set_english(chewing_plus_session *session, int enabled,
                              int hint, const char *shortcut_label) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    s->opt.english = enabled != 0;
    s->opt.englishHint = hint != 0;
    if (shortcut_label) {
        s->opt.englishShortcutLabel = shortcut_label;
    }
    s->applyOptions();
    s->reloadData();
}

void chewing_plus_set_english_hint_commit(chewing_plus_session *session,
                                          int enabled) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    s->opt.englishHintCommit = enabled != 0;
    s->applyOptions();
}

void chewing_plus_set_english_combo(chewing_plus_session *session,
                                    int enabled) {
    if (session) {
        S(session)->englishCombo = enabled != 0;
    }
}

void chewing_plus_set_shift_toggle(chewing_plus_session *session, int enabled) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    s->shiftToggle = enabled != 0;
    s->shiftPending = false;
    if (!s->shiftToggle) {
        s->setAscii(false);
    }
}

int chewing_plus_ascii_mode(const chewing_plus_session *session) {
    return session && S(session)->asciiMode ? 1 : 0;
}

void chewing_plus_set_ascii_mode(chewing_plus_session *session, int enabled) {
    if (session) {
        S(session)->setAscii(enabled != 0);
    }
}

int chewing_plus_page(chewing_plus_session *session, int next) {
    return session && S(session)->page(next != 0) ? 1 : 0;
}

void chewing_plus_flush(chewing_plus_session *session, int mode) {
    if (session) {
        S(session)->flush(mode);
    }
}

void chewing_plus_set_english_double_tap(chewing_plus_session *session,
                                         int enabled, int window_ms) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    s->englishDoubleTap = enabled != 0;
    if (window_ms > 0) {
        s->englishTap.setWindowMs(window_ms);
    }
    s->englishTap.reset();
}

void chewing_plus_set_sensitive(chewing_plus_session *session, int enabled) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    s->sensitive = enabled != 0;
    if (s->sensitive) {
        // Drop an open Plus panel. The Chewing buffer is left alone; the
        // frontend resets the session when the client actually changes.
        s->plus.reset();
        s->englishTap.reset();
    }
    s->refresh();
}

int chewing_plus_sensitive(const chewing_plus_session *session) {
    return session && S(session)->sensitive ? 1 : 0;
}

void chewing_plus_set_keyboard_layout(chewing_plus_session *session,
                                      const char *kb_name) {
    if (!session || !kb_name) {
        return;
    }
    auto *s = S(session);
    s->layout = kb_name;
    chewing_set_KBType(s->raw(), chewing_KBStr2Num(kb_name));
}

void chewing_plus_set_selection_keys(chewing_plus_session *session,
                                     const char *ten_keys) {
    if (!session || !ten_keys || std::strlen(ten_keys) < 10) {
        return;
    }
    auto *s = S(session);
    s->selKeys.assign(ten_keys, 10);
    int sel[10];
    for (int i = 0; i < 10; i++) {
        sel[i] = static_cast<unsigned char>(s->selKeys[i]);
    }
    chewing_set_selKey(s->raw(), sel, 10);
}

void chewing_plus_set_page_size(chewing_plus_session *session, int page_size) {
    if (!session || page_size < 1 || page_size > 10) {
        return;
    }
    chewing_set_candPerPage(S(session)->raw(), page_size);
}

void chewing_plus_set_easy_symbol(chewing_plus_session *session, int enabled) {
    if (!session) {
        return;
    }
    S(session)->easySymbol = enabled != 0;
}

void chewing_plus_set_space_as_selection(chewing_plus_session *session,
                                         int enabled) {
    if (!session) {
        return;
    }
    chewing_set_spaceAsSelection(S(session)->raw(), enabled ? 1 : 0);
}

void chewing_plus_set_phrase_options(chewing_plus_session *session,
                                     int add_phrase_forward,
                                     int choice_backward,
                                     int auto_shift_cursor) {
    if (!session) {
        return;
    }
    ChewingContext *c = S(session)->raw();
    chewing_set_addPhraseDirection(c, add_phrase_forward ? 0 : 1);
    chewing_set_phraseChoiceRearward(c, choice_backward ? 1 : 0);
    chewing_set_autoShiftCur(c, auto_shift_cursor ? 1 : 0);
}

int chewing_plus_key(chewing_plus_session *session,
                     const chewing_plus_key_event *event) {
    if (!session || !event) {
        return 0;
    }
    return S(session)->handle(*event);
}

int chewing_plus_toggle_english(chewing_plus_session *session) {
    if (!session) {
        return 0;
    }
    auto *s = S(session);
    if (!s->toggleEnglish()) {
        return 0;
    }
    s->refresh();
    return 1;
}

void chewing_plus_insert_text(chewing_plus_session *session, const char *utf8) {
    if (!session || !utf8 || !utf8[0]) {
        return;
    }
    auto *s = S(session);
    if (s->sensitive) {
        return;
    }
    s->plus.insertDictation(s->raw(), utf8,
                            [s](const std::string &t) { s->commitText(t); });
    s->refresh();
}

void chewing_plus_reset(chewing_plus_session *session) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    ChewingContext *c = s->raw();
    s->plus.reset();
    s->englishTap.reset();
    chewing_cand_close(c);
    chewing_clean_preedit_buf(c);
    chewing_clean_bopomofo_buf(c);
    chewing_Reset(c);
    s->refresh();
}

const char *chewing_plus_preedit(chewing_plus_session *session, size_t *cursor,
                                 size_t *highlight_start,
                                 size_t *highlight_length) {
    if (!session) {
        return nullptr;
    }
    auto *s = S(session);
    if (cursor) {
        *cursor = s->cursor;
    }
    if (highlight_start) {
        *highlight_start = s->hlStart;
    }
    if (highlight_length) {
        *highlight_length = s->hlLen;
    }
    return s->preedit.c_str();
}

const char *chewing_plus_aux(chewing_plus_session *session) {
    return session ? S(session)->aux.c_str() : nullptr;
}

int chewing_plus_panel(const chewing_plus_session *session) {
    return session ? S(session)->panelKind : CHEWING_PLUS_PANEL_NONE;
}

const char *chewing_plus_panel_input(const chewing_plus_session *session) {
    return session ? S(session)->panelInput.c_str() : nullptr;
}

int chewing_plus_candidate_count(const chewing_plus_session *session) {
    return session ? static_cast<int>(S(session)->cands.size()) : 0;
}

const char *chewing_plus_candidate(const chewing_plus_session *session,
                                   int index) {
    if (!session || index < 0 ||
        index >= static_cast<int>(S(session)->cands.size())) {
        return nullptr;
    }
    return S(session)->cands[index].text.c_str();
}

const char *chewing_plus_candidate_label(const chewing_plus_session *session,
                                         int index) {
    if (!session || index < 0 ||
        index >= static_cast<int>(S(session)->cands.size())) {
        return nullptr;
    }
    return S(session)->cands[index].label.c_str();
}

int chewing_plus_candidate_kind(const chewing_plus_session *session,
                                int index) {
    if (!session || index < 0 ||
        index >= static_cast<int>(S(session)->cands.size())) {
        return -1;
    }
    return S(session)->cands[index].kind;
}

void chewing_plus_select(chewing_plus_session *session, int index) {
    if (!session) {
        return;
    }
    auto *s = S(session);
    if (index < 0 || index >= static_cast<int>(s->cands.size())) {
        return;
    }
    const Candidate cand = s->cands[index];
    ChewingContext *c = s->raw();
    if (cand.kind == CHEWING_PLUS_CAND_PANEL) {
        s->plus.select(c, cand.index,
                       [s](const std::string &t) { s->commitText(t); });
    } else if (cand.kind == CHEWING_PLUS_CAND_EMOJI) {
        s->selectEmoji(c, cand.text);
    } else {
        s->selectChewing(c, cand.index);
    }
    s->refresh();
}

int chewing_plus_setting_count(void) {
    return static_cast<int>(sizeof(kSettings) / sizeof(kSettings[0]));
}

int chewing_plus_setting_info_at(int index, chewing_plus_setting_info *out) {
    if (!out || index < 0 || index >= chewing_plus_setting_count()) {
        return 0;
    }
    const auto &row = kSettings[index];
    out->key = row.key;
    out->type = row.type;
    out->default_value = row.defaultValue;
    out->label_en = row.labelEn;
    out->label_zh = row.labelZh;
    return 1;
}

int chewing_plus_set_setting(chewing_plus_session *session, const char *key,
                             const char *value) {
    const SettingRow *row = FindSetting(key);
    if (!session || !row || !value) {
        return 0;
    }
    std::string normalized;
    if (row->type == CHEWING_PLUS_SETTING_BOOL) {
        normalized = ParseBool(value);
        if (normalized.empty()) {
            return 0;
        }
    } else if (row->type == CHEWING_PLUS_SETTING_INT) {
        int n = 0;
        if (!ParseInt(value, n)) {
            return 0;
        }
        normalized = std::to_string(n);
    } else {
        normalized = value;
    }
    S(session)->settings[row->key] = normalized;
    ApplySetting(session, row->key);
    return 1;
}

const char *chewing_plus_get_setting(chewing_plus_session *session,
                                     const char *key) {
    const SettingRow *row = FindSetting(key);
    if (!session || !row) {
        return nullptr;
    }
    auto *s = S(session);
    s->settingOut = s->settings[row->key];
    return s->settingOut.c_str();
}

int chewing_plus_get_view(chewing_plus_session *session,
                          chewing_plus_view *out) {
    if (!session || !out) {
        return 0;
    }
    auto *s = S(session);
    out->revision = s->revision;
    out->preedit = s->preedit.c_str();
    out->cursor = s->cursor;
    out->highlight_start = s->hlStart;
    out->highlight_length = s->hlLen;
    out->aux = s->aux.c_str();
    out->panel = s->panelKind;
    out->candidate_count = static_cast<int>(s->cands.size());
    out->suggestion_count = static_cast<int>(s->sugg.size());
    return 1;
}

const char *chewing_plus_take_commit(chewing_plus_session *session) {
    if (!session) {
        return nullptr;
    }
    auto *s = S(session);
    s->takenCommit = std::move(s->pendingCommit);
    s->pendingCommit.clear();
    return s->takenCommit.c_str();
}

char **chewing_plus_user_phrases(chewing_plus_session *session, size_t *count) {
    if (count) {
        *count = 0;
    }
    if (!session) {
        return nullptr;
    }
    ChewingContext *c = S(session)->raw();
    if (chewing_userphrase_enumerate(c) != 0) {
        return nullptr;
    }
    std::vector<std::string> words;
    unsigned int plen = 0;
    unsigned int blen = 0;
    while (chewing_userphrase_has_next(c, &plen, &blen) &&
           words.size() < 2000) {
        std::string phrase(plen, '\0');
        std::string bopomofo(blen, '\0');
        if (chewing_userphrase_get(c, phrase.data(), plen, bopomofo.data(),
                                   blen) != 0) {
            break;
        }
        phrase.resize(std::strlen(phrase.c_str()));
        if (CodePoints(phrase) >= 2) {
            words.push_back(std::move(phrase));
        }
    }
    std::stable_sort(words.begin(), words.end(),
                     [](const std::string &a, const std::string &b) {
                         return a.size() > b.size();
                     });
    if (words.size() > 80) {
        words.resize(80);
    }
    if (words.empty()) {
        return nullptr;
    }
    char **list =
        static_cast<char **>(std::calloc(words.size() + 1, sizeof(char *)));
    if (!list) {
        return nullptr;
    }
    for (size_t i = 0; i < words.size(); i++) {
        list[i] = strdup(words[i].c_str());
    }
    if (count) {
        *count = words.size();
    }
    return list;
}

void chewing_plus_free_strings(char **list) {
    if (!list) {
        return;
    }
    for (char **p = list; *p; ++p) {
        free(*p);
    }
    free(list);
}

} // extern "C"
