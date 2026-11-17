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
#include <string>
#include <vector>

namespace chewing_plus {

class PhraseIndex {
public:
    // "phrase<TAB>freq<TAB>abbr" lines, '#' comments. Returns phrases read.
    size_t load(const std::string &path);
    void clear();
    size_t size() const { return phrases_.size(); }
    // Build both sorted indexes. Call after the last load(); a finalized
    // index is safe to share between sessions (lookups do not write).
    void finalize() { sortIfNeeded(); }

    // Phrases that start with `typed` and are longer, most frequent first.
    // `typed` must be at least `minChars` characters (UTF-8 code points).
    std::vector<std::string> complete(const std::string &typed,
                                      size_t limit = 9,
                                      size_t minChars = 2) const;

    // Phrases whose syllables start with these Bopomofo symbols, in order
    // (ㄍㄒㄈㄘ). At least `minSyllables` symbols; most frequent first.
    std::vector<std::string> expand(const std::string &initials,
                                    size_t limit = 9,
                                    size_t minSyllables = 3) const;

private:
    struct Phrase {
        std::string text;
        std::string abbr;
        uint32_t freq;
    };
    void sortIfNeeded() const;

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
