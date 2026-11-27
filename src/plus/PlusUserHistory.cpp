/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "PlusUserHistory.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace chewing_plus {
namespace {

constexpr size_t kMaxPhraseEntries = 10000;
constexpr size_t kMaxCharEntries = 2000;
constexpr double kMaxCount = 10000.0;
constexpr double kFreqHalfLifeDays = 30.0;
constexpr double kRecencyHalfLifeDays = 7.0;
constexpr double kFreqWeight = 4.0;
constexpr double kRecencyWeight = 4.0;
// Baseline score for a libchewing user-dict phrase not in the static table
// (~log2(256)), so it surfaces cleanly before being picked.
constexpr double kUserDictBaseline = 8.0;
constexpr size_t kDebounceOps = 10;
constexpr int64_t kDebounceSec = 30;

bool StartsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool ValidText(std::string_view s) {
    if (s.empty() || s.size() > 120) {
        return false;
    }
    for (char c : s) {
        if (c == '\t' || c == '\n' || c == '\r' || c == '\0') {
            return false;
        }
    }
    return true;
}

} // namespace

UserHistory::~UserHistory() { saveIfDirty(nowSeconds(), true); }

int64_t UserHistory::nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

double UserHistory::decayCount(double count, int64_t lastUsedSec,
                               int64_t nowSec) {
    if (count <= 0.0) {
        return 0.0;
    }
    if (lastUsedSec <= 0 || nowSec <= lastUsedSec) {
        return count;
    }
    const double dtDays =
        static_cast<double>(nowSec - lastUsedSec) / 86400.0;
    return count * std::exp2(-dtDays / kFreqHalfLifeDays);
}

double UserHistory::computeScore(const Entry *entry, uint32_t baseFreq,
                                 int64_t nowSec) {
    const double base = std::log2(1.0 + static_cast<double>(baseFreq));
    if (!entry) {
        return base;
    }
    const double effCount =
        decayCount(entry->count, entry->lastUsedSec, nowSec);
    double recency = 0.0;
    if (entry->count > 0.0 && entry->lastUsedSec > 0) {
        const double dtDays =
            nowSec > entry->lastUsedSec
                ? static_cast<double>(nowSec - entry->lastUsedSec) / 86400.0
                : 0.0;
        recency = std::exp2(-dtDays / kRecencyHalfLifeDays);
    }
    const double userDictBonus =
        (entry->fromUserDict && baseFreq == 0) ? kUserDictBaseline : 0.0;
    return base + kFreqWeight * std::log2(1.0 + effCount) +
           kRecencyWeight * recency + userDictBonus;
}

void UserHistory::loadFromFileLocked(const std::string &path, bool merge) {
    if (!merge) {
        phrases_.clear();
        chars_.clear();
    }
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) {
        if (!merge) {
            loadedMtime_ = 0;
            loadedSize_ = -1;
        }
        return;
    }
    std::ifstream in(path);
    if (!in) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // kind<TAB>text<TAB>abbr<TAB>count<TAB>last_used_epoch
        size_t t1 = line.find('\t');
        size_t t2 = t1 == std::string::npos ? t1 : line.find('\t', t1 + 1);
        size_t t3 = t2 == std::string::npos ? t2 : line.find('\t', t2 + 1);
        size_t t4 = t3 == std::string::npos ? t3 : line.find('\t', t3 + 1);
        if (t4 == std::string::npos) {
            continue;
        }
        std::string_view kindStr(line.data(), t1);
        if (kindStr != "p" && kindStr != "c") {
            continue;
        }
        std::string text = line.substr(t1 + 1, t2 - t1 - 1);
        std::string abbr = line.substr(t2 + 1, t3 - t2 - 1);
        if (!ValidText(text)) {
            continue;
        }
        double count = 0.0;
        int64_t ts = 0;
        try {
            count = std::stod(line.substr(t3 + 1, t4 - t3 - 1));
            ts = std::stoll(line.substr(t4 + 1));
        } catch (...) {
            continue;
        }
        if (!std::isfinite(count) || count < 0.0 || ts < 0) {
            continue;
        }
        count = std::min(count, kMaxCount);
        auto &table = (kindStr == "p") ? phrases_ : chars_;
        auto it = table.find(text);
        if (it == table.end()) {
            Entry e;
            e.text = text;
            e.abbr = std::move(abbr);
            e.count = count;
            e.lastUsedSec = ts;
            table.emplace(std::move(text), std::move(e));
        } else if (merge) {
            // Merge with another process's newer save: keep the fresher state
            // and higher decayed count.
            int64_t ref = std::max(it->second.lastUsedSec, ts);
            double mine = decayCount(it->second.count, it->second.lastUsedSec, ref);
            double theirs = decayCount(count, ts, ref);
            it->second.count = std::min(std::max(mine, theirs), kMaxCount);
            it->second.lastUsedSec = ref;
            if (it->second.abbr.empty() && !abbr.empty()) {
                it->second.abbr = std::move(abbr);
            }
        }
    }
    loadedMtime_ = static_cast<int64_t>(st.st_mtime);
    loadedSize_ = static_cast<int64_t>(st.st_size);
}

size_t UserHistory::load(const std::string &path) {
    std::unique_lock<std::shared_mutex> lock(mu_);
    path_ = path;
    dirtyOps_ = 0;
    lastSaveSec_ = nowSeconds();
    if (path.empty()) {
        phrases_.clear();
        chars_.clear();
        return 0;
    }
    loadFromFileLocked(path, false);
    return phrases_.size() + chars_.size();
}

void UserHistory::record(Kind kind, std::string_view text,
                         std::string_view abbr, double weight, int64_t nowSec) {
    if (!ValidText(text) || weight <= 0.0) {
        return;
    }
    if (nowSec <= 0) {
        nowSec = nowSeconds();
    }
    std::unique_lock<std::shared_mutex> lock(mu_);
    auto &table = (kind == Kind::Phrase) ? phrases_ : chars_;
    std::string key(text);
    auto &e = table[key];
    if (e.text.empty()) {
        e.text = key;
    }
    if (!abbr.empty() && e.abbr.empty()) {
        e.abbr.assign(abbr);
    }
    double decayed = decayCount(e.count, e.lastUsedSec, nowSec);
    e.count = std::min(decayed + weight, kMaxCount);
    e.lastUsedSec = std::max(e.lastUsedSec, nowSec);
    ++dirtyOps_;

    const size_t limit =
        (kind == Kind::Phrase) ? kMaxPhraseEntries : kMaxCharEntries;
    if (table.size() > limit + limit / 10) {
        pruneLocked(table, limit, nowSec);
    }
}

void UserHistory::importUserPhrases(
    const std::vector<std::pair<std::string, std::string>> &phrases) {
    std::unique_lock<std::shared_mutex> lock(mu_);
    for (const auto &[text, abbr] : phrases) {
        if (!ValidText(text)) {
            continue;
        }
        auto &e = phrases_[text];
        if (e.text.empty()) {
            e.text = text;
        }
        if (!abbr.empty()) {
            e.abbr = abbr;
        }
        e.fromUserDict = true;
    }
}

void UserHistory::clear() {
    std::unique_lock<std::shared_mutex> lock(mu_);
    phrases_.clear();
    chars_.clear();
    dirtyOps_ = 0;
    if (!path_.empty()) {
        std::remove(path_.c_str());
        loadedMtime_ = 0;
        loadedSize_ = -1;
    }
}

void UserHistory::pruneLocked(std::unordered_map<std::string, Entry> &table,
                              size_t maxEntries, int64_t nowSec) {
    if (table.size() <= maxEntries) {
        return;
    }
    std::vector<std::pair<double, std::string>> scored;
    scored.reserve(table.size());
    for (const auto &[k, e] : table) {
        // Keep libchewing user-dict entries intact; prune low-score history.
        double s = e.fromUserDict ? 1e9 : computeScore(&e, 0, nowSec);
        scored.emplace_back(s, k);
    }
    const size_t drop = table.size() - maxEntries;
    std::nth_element(scored.begin(), scored.begin() + static_cast<long>(drop),
                     scored.end(), [](const auto &a, const auto &b) {
                         return a.first < b.first;
                     });
    for (size_t i = 0; i < drop; ++i) {
        if (scored[i].first < 1e8) {
            table.erase(scored[i].second);
        }
    }
}

bool UserHistory::saveIfDirty(int64_t nowSec, bool force) {
    std::unique_lock<std::shared_mutex> lock(mu_);
    if (path_.empty() || dirtyOps_ == 0) {
        return true;
    }
    if (nowSec <= 0) {
        nowSec = nowSeconds();
    }
    if (!force && dirtyOps_ < kDebounceOps &&
        (nowSec - lastSaveSec_) < kDebounceSec) {
        return true;
    }

    // If another process updated the file on disk since our last load/save,
    // merge before writing so neither process loses records.
    struct stat st{};
    if (stat(path_.c_str(), &st) == 0) {
        if (static_cast<int64_t>(st.st_mtime) != loadedMtime_ ||
            static_cast<int64_t>(st.st_size) != loadedSize_) {
            loadFromFileLocked(path_, true);
        }
    }

    pruneLocked(phrases_, kMaxPhraseEntries, nowSec);
    pruneLocked(chars_, kMaxCharEntries, nowSec);

    std::error_code ec;
    auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }
    std::string tmp =
        path_ + ".tmp." + std::to_string(static_cast<long long>(getpid()));
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << "# chewing-plus usage v1\n";
        out << "# kind\ttext\tabbr\tcount\tlast_used_epoch\n";
        auto writeTable = [&](char kindChar,
                              const std::unordered_map<std::string, Entry> &t) {
            for (const auto &[k, e] : t) {
                if (e.count <= 0.01) {
                    continue;
                }
                char num[32];
                std::snprintf(num, sizeof(num), "%.2f", e.count);
                out << kindChar << '\t' << e.text << '\t' << e.abbr << '\t'
                    << num << '\t' << e.lastUsedSec << '\n';
            }
        };
        writeTable('p', phrases_);
        writeTable('c', chars_);
        out.flush();
        if (!out) {
            std::remove(tmp.c_str());
            return false;
        }
    }
    if (std::rename(tmp.c_str(), path_.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    if (stat(path_.c_str(), &st) == 0) {
        loadedMtime_ = static_cast<int64_t>(st.st_mtime);
        loadedSize_ = static_cast<int64_t>(st.st_size);
    }
    dirtyOps_ = 0;
    lastSaveSec_ = nowSec;
    return true;
}

bool UserHistory::empty() const {
    std::shared_lock<std::shared_mutex> lock(mu_);
    return phrases_.empty() && chars_.empty();
}

size_t UserHistory::phraseCount() const {
    std::shared_lock<std::shared_mutex> lock(mu_);
    return phrases_.size();
}

size_t UserHistory::charCount() const {
    std::shared_lock<std::shared_mutex> lock(mu_);
    return chars_.size();
}

bool UserHistory::hasPhrase(std::string_view text) const {
    std::shared_lock<std::shared_mutex> lock(mu_);
    return phrases_.find(std::string(text)) != phrases_.end();
}

double UserHistory::score(Kind kind, std::string_view text, uint32_t baseFreq,
                          int64_t nowSec) const {
    std::shared_lock<std::shared_mutex> lock(mu_);
    const auto &table = (kind == Kind::Phrase) ? phrases_ : chars_;
    auto it = table.find(std::string(text));
    const Entry *e = (it == table.end()) ? nullptr : &it->second;
    return computeScore(e, baseFreq, nowSec);
}

std::vector<UserHistory::ExtraItem>
UserHistory::matchCompletions(std::string_view typed, int64_t nowSec) const {
    std::shared_lock<std::shared_mutex> lock(mu_);
    std::vector<ExtraItem> out;
    for (const auto &[k, e] : phrases_) {
        if (e.text.size() > typed.size() && StartsWith(e.text, typed)) {
            out.push_back({e.text, e.abbr, computeScore(&e, 0, nowSec)});
        }
    }
    return out;
}

std::vector<UserHistory::ExtraItem>
UserHistory::matchExpansions(std::string_view initials, int64_t nowSec) const {
    std::shared_lock<std::shared_mutex> lock(mu_);
    std::vector<ExtraItem> out;
    for (const auto &[k, e] : phrases_) {
        if (!e.abbr.empty() && e.abbr == initials) {
            out.push_back({e.text, e.abbr, computeScore(&e, 0, nowSec)});
        }
    }
    return out;
}

std::vector<UserHistory::ExtraItem>
UserHistory::matchReadings(std::string_view reading, bool exact,
                           int64_t nowSec) const {
    std::shared_lock<std::shared_mutex> lock(mu_);
    std::vector<ExtraItem> out;
    for (const auto &[k, e] : chars_) {
        if (e.abbr.empty()) {
            continue;
        }
        bool match = exact ? (e.abbr == reading)
                           : (e.abbr != reading && StartsWith(e.abbr, reading));
        if (match) {
            out.push_back({e.text, e.abbr, computeScore(&e, 0, nowSec)});
        }
    }
    return out;
}

} // namespace chewing_plus
