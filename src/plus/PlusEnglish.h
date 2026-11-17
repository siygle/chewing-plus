// SPDX-License-Identifier: MIT
// English buffer mode lexicon for McBopomofo Plus.
#ifndef SRC_PLUSENGLISH_H_
#define SRC_PLUSENGLISH_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace chewing_plus {

// Case-insensitive word lookup with canonical casing (github -> GitHub).
// Built-in words load first; the user file loads second and wins.
//
// Storage is one sorted array of (lowercase key, casing) entries pointing
// into a single string pool, so 70k words cost a few MB instead of a hash
// map plus two copies of every word.
class EnglishLexicon {
public:
    // One word per line, '#' comments. Returns the number of words read.
    size_t load(const std::string &path, bool userFile = false);
    void clear();
    size_t size() const { return count_; }
    // Case-insensitive membership.
    bool contains(const std::string &word) const;

    // Candidates for what the user typed, in order:
    //  1. user-defined casing of the exact word,
    //  2. the text exactly as typed,
    //  3. other dictionary casings of the exact word (e.g. GitHub, Apple),
    //  4. completions: user words first, then built-in, shortest first.
    // Always non-empty for non-empty input; never contains duplicates.
    std::vector<std::string> candidates(const std::string &typed,
                                        size_t limit = 9) const;

    // The canonical form to commit with Space: user casing, else the single
    // dictionary casing when the user typed all-lowercase, else as typed.
    std::string canonical(const std::string &typed) const;

    // Sort the index. Call after the last load(); a finalized lexicon is
    // safe to share between sessions (lookups do not write).
    void finalize();

private:
    struct Entry {
        uint32_t key;   // offset of the lowercase form in pool_
        uint32_t text;  // offset of this casing in pool_
        uint16_t len;   // byte length (same for key and text)
        uint8_t user;   // from the user file
        uint32_t order; // load order within the same key
    };
    std::string_view keyOf(const Entry &e) const;
    std::string_view textOf(const Entry &e) const;
    // [first, last) entries for this lowercase key, user casing first.
    std::pair<const Entry *, const Entry *> range(std::string_view key) const;
    void sortIfNeeded() const;

    std::string pool_;
    mutable std::vector<Entry> entries_;
    mutable bool sorted_ = true;
    size_t count_ = 0;
    uint32_t nextOrder_ = 0;
};

} // namespace chewing_plus
#endif
