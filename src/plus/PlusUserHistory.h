/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
// Per-user usage history for adaptive phrase and character suggestion ranking.
//
// Read-only base indexes (plus-phrases.bin / plus-chars.bin) remain mmap'd and
// shared across processes. UserHistory sits on top as a compact, bounded
// overlay that tracks frecency (decayed frequency + recency) and imports
// custom phrases from libchewing's user dictionary so they also appear in
// completions and initial-only expansions.
#ifndef SRC_PLUSUSERHISTORY_H_
#define SRC_PLUSUSERHISTORY_H_

#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace chewing_plus {

class UserHistory {
public:
    enum class Kind { Phrase, Char };

    struct ExtraItem {
        std::string text;
        std::string abbr;
        double score = 0.0;
    };

    UserHistory() = default;
    ~UserHistory();
    UserHistory(const UserHistory &) = delete;
    UserHistory &operator=(const UserHistory &) = delete;

    // Load usage records from `path` (TSV). Remembers `path` for subsequent
    // saves. Missing file is not an error.
    size_t load(const std::string &path);
    const std::string &path() const { return path_; }

    // Record usage of a phrase or single character. `abbr` is optional
    // (Bopomofo initials for Phrase, toneless reading for Char).
    void record(Kind kind, std::string_view text, std::string_view abbr,
                double weight, int64_t nowSec);

    // Import (phrase, bopomofo_initials) pairs from libchewing's user
    // dictionary so custom phrases surface in suggestions immediately.
    void importUserPhrases(
        const std::vector<std::pair<std::string, std::string>> &phrases);

    // Clear all learned usage (in memory and on disk if a path is set).
    // Preserves imported libchewing user phrases' baseline presence only if
    // re-imported later.
    void clear();

    // Flush dirty changes to disk atomically. When `force` is false, flushes
    // only after enough updates or elapsed seconds (debounce).
    bool saveIfDirty(int64_t nowSec, bool force = false);

    bool empty() const;
    size_t phraseCount() const;
    size_t charCount() const;
    bool hasPhrase(std::string_view text) const;

    // Combined Frecency score for a candidate with static `baseFreq` at
    // `nowSec`. Higher is better.
    double score(Kind kind, std::string_view text, uint32_t baseFreq,
                 int64_t nowSec) const;

    // Entries stored in UserHistory that match a completion, initial expansion,
    // or partial reading query (used to surface custom / user-dict phrases not
    // in the static base index).
    std::vector<ExtraItem> matchCompletions(std::string_view typed,
                                            int64_t nowSec) const;
    std::vector<ExtraItem> matchExpansions(std::string_view initials,
                                           int64_t nowSec) const;
    std::vector<ExtraItem> matchReadings(std::string_view reading, bool exact,
                                         int64_t nowSec) const;

    // Current wall-clock seconds since Unix epoch.
    static int64_t nowSeconds();

private:
    struct Entry {
        std::string text;
        std::string abbr;
        double count = 0.0;
        int64_t lastUsedSec = 0;
        bool fromUserDict = false;
    };

    static double decayCount(double count, int64_t lastUsedSec, int64_t nowSec);
    static double computeScore(const Entry *entry, uint32_t baseFreq,
                               int64_t nowSec);
    void loadFromFileLocked(const std::string &path, bool merge);
    void pruneLocked(std::unordered_map<std::string, Entry> &table,
                     size_t maxEntries, int64_t nowSec);

    mutable std::shared_mutex mu_;
    std::string path_;
    std::unordered_map<std::string, Entry> phrases_;
    std::unordered_map<std::string, Entry> chars_;
    size_t dirtyOps_ = 0;
    int64_t lastSaveSec_ = 0;
    int64_t loadedMtime_ = 0;
    int64_t loadedSize_ = -1;
};

} // namespace chewing_plus

#endif // SRC_PLUSUSERHISTORY_H_
