/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
// Phrase suggestions while typing: complete what is already in the buffer
// (中華 -> 中華民國) and expand initial-only Bopomofo (ㄍㄒㄈㄘ -> 恭喜發財).
// Data: data/plus-phrases.txt (scripts/make-phrases.py). Pure logic, no
// libchewing dependency, so it is unit tested directly.
#ifndef SRC_PLUSPHRASES_H_
#define SRC_PLUSPHRASES_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace chewing_plus {

// Either the text format (parsed and sorted in memory) or the compiled
// format from writeCompiled() (memory-mapped read-only: nothing parsed,
// pages shared between processes). load() tells them apart by the header.
class PhraseIndex {
public:
    PhraseIndex() = default;
    ~PhraseIndex();
    PhraseIndex(const PhraseIndex &) = delete;
    PhraseIndex &operator=(const PhraseIndex &) = delete;

    // Text: "phrase<TAB>freq<TAB>abbr" lines, '#' comments. Or a compiled
    // file. Returns the number of phrases.
    //
    // `readings`: the third column is a whole toneless reading (好 ㄏㄠ,
    // data/plus-chars.txt) instead of one initial per character.
    size_t load(const std::string &path, bool readings = false);
    void clear();
    size_t size() const;
    bool compiled() const { return map_ != nullptr; }
    // Write the compiled form of a text index. Returns false on error.
    bool writeCompiled(const std::string &path) const;
    // Build both sorted indexes. Call after the last load(); a finalized
    // index is safe to share between sessions (lookups do not write).
    void finalize() { sortIfNeeded(); }

    // Phrases that start with `typed` and are longer, most frequent first.
    // `typed` must be at least `minChars` characters (UTF-8 code points).
    std::vector<std::string> complete(const std::string &typed,
                                      size_t limit = 9,
                                      size_t minChars = 2) const;

    // Entries whose reading starts with `typed` (a partly typed syllable:
    // ㄋ -> 你 年 您, ㄋㄧ -> 你 妳 尼), most frequent first. For an index
    // loaded with readings = true.
    std::vector<std::string> byReading(const std::string &typed,
                                       size_t limit = 9) const;

    // Phrases whose syllables start with these Bopomofo symbols, in order
    // (ㄍㄒㄈㄘ). At least `minSyllables` symbols; most frequent first.
    std::vector<std::string> expand(const std::string &initials,
                                    size_t limit = 9,
                                    size_t minSyllables = 2) const;

private:
    struct Phrase {
        std::string text;
        std::string abbr;
        uint32_t freq;
    };
    void sortIfNeeded() const;
    bool loadCompiled(const std::string &path);
    void unmap();
    std::string_view text(uint32_t i) const;
    std::string_view abbr(uint32_t i) const;
    uint32_t freq(uint32_t i) const;
    const uint32_t *order(bool byAbbr) const;
    std::vector<std::string>
    collect(bool byAbbr, std::string_view key, size_t limit,
            const std::function<bool(uint32_t)> &accept) const;

    const void *map_ = nullptr;
    size_t mapSize_ = 0;
    std::vector<Phrase> phrases_;
    // Indexes into phrases_, sorted by text / by abbreviation.
    mutable std::vector<uint32_t> byText_;
    mutable std::vector<uint32_t> byAbbr_;
    mutable bool sorted_ = true;
};

// True when every code point of `s` is a Bopomofo initial-capable symbol
// (U+3105..U+3129), i.e. the reading could be an abbreviation.
bool AllBopomofo(const std::string &s);

} // namespace chewing_plus
#endif
