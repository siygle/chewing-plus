/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "PlusPhrases.h"

#include <algorithm>
#include <fstream>
#include <functional>

namespace chewing_plus {
namespace {
constexpr size_t kMaxPhrases = 400000;
constexpr size_t kMaxPhraseBytes = 120;

size_t CodePointCount(const std::string &s) {
    return std::count_if(s.begin(), s.end(),
                         [](unsigned char c) { return (c & 0xC0) != 0x80; });
}

bool StartsWith(const std::string &s, const std::string &prefix) {
    return s.size() >= prefix.size() &&
           s.compare(0, prefix.size(), prefix) == 0;
}

// Collect phrases from a sorted index range that starts with `key`, then
// keep the `limit` most frequent (ties: shorter, then text order).
template <typename KeyOf>
std::vector<std::string>
Collect(const std::vector<uint32_t> &index, const std::string &key, KeyOf keyOf,
        size_t limit, const std::function<bool(uint32_t)> &accept,
        const std::function<std::pair<uint32_t, const std::string *>(uint32_t)>
            &rank) {
    auto lo = std::lower_bound(
        index.begin(), index.end(), key,
        [&](uint32_t i, const std::string &k) { return keyOf(i) < k; });
    std::vector<uint32_t> hits;
    for (auto it = lo; it != index.end() && StartsWith(keyOf(*it), key); ++it) {
        if (accept(*it)) {
            hits.push_back(*it);
        }
    }
    std::sort(hits.begin(), hits.end(), [&](uint32_t a, uint32_t b) {
        auto [fa, ta] = rank(a);
        auto [fb, tb] = rank(b);
        if (fa != fb)
            return fa > fb;
        if (ta->size() != tb->size())
            return ta->size() < tb->size();
        return *ta < *tb;
    });
    std::vector<std::string> out;
    for (size_t i = 0; i < hits.size() && out.size() < limit; ++i) {
        out.push_back(*rank(hits[i]).second);
    }
    return out;
}
} // namespace

bool AllBopomofo(const std::string &s) {
    if (s.empty())
        return false;
    // U+3105..U+3129 encode as E3 84 85 .. E3 84 A9.
    if (s.size() % 3 != 0)
        return false;
    for (size_t i = 0; i < s.size(); i += 3) {
        auto b0 = static_cast<unsigned char>(s[i]);
        auto b1 = static_cast<unsigned char>(s[i + 1]);
        auto b2 = static_cast<unsigned char>(s[i + 2]);
        if (b0 != 0xE3 || b1 != 0x84 || b2 < 0x85 || b2 > 0xA9)
            return false;
    }
    return true;
}

void PhraseIndex::clear() {
    phrases_.clear();
    byText_.clear();
    byAbbr_.clear();
    sorted_ = true;
}

size_t PhraseIndex::load(const std::string &path) {
    std::ifstream in(path);
    if (!in)
        return 0;
    size_t added = 0;
    std::string line;
    while (std::getline(in, line) && phrases_.size() < kMaxPhrases) {
        if (line.empty() || line[0] == '#')
            continue;
        auto t1 = line.find('\t');
        auto t2 = t1 == std::string::npos ? t1 : line.find('\t', t1 + 1);
        if (t2 == std::string::npos)
            continue;
        Phrase p;
        p.text = line.substr(0, t1);
        p.abbr = line.substr(t2 + 1);
        if (!p.abbr.empty() && p.abbr.back() == '\r')
            p.abbr.pop_back();
        try {
            p.freq = static_cast<uint32_t>(
                std::stoul(line.substr(t1 + 1, t2 - t1 - 1)));
        } catch (...) {
            continue;
        }
        if (p.text.empty() || p.text.size() > kMaxPhraseBytes ||
            !AllBopomofo(p.abbr) ||
            CodePointCount(p.abbr) != CodePointCount(p.text))
            continue;
        phrases_.push_back(std::move(p));
        ++added;
    }
    sorted_ = false;
    return added;
}

void PhraseIndex::sortIfNeeded() const {
    if (sorted_)
        return;
    byText_.resize(phrases_.size());
    byAbbr_.resize(phrases_.size());
    for (uint32_t i = 0; i < phrases_.size(); ++i) {
        byText_[i] = i;
        byAbbr_[i] = i;
    }
    std::sort(byText_.begin(), byText_.end(), [this](uint32_t a, uint32_t b) {
        return phrases_[a].text < phrases_[b].text;
    });
    std::sort(byAbbr_.begin(), byAbbr_.end(), [this](uint32_t a, uint32_t b) {
        return phrases_[a].abbr < phrases_[b].abbr;
    });
    sorted_ = true;
}

std::vector<std::string> PhraseIndex::complete(const std::string &typed,
                                               size_t limit,
                                               size_t minChars) const {
    if (CodePointCount(typed) < minChars || phrases_.empty())
        return {};
    sortIfNeeded();
    return Collect(
        byText_, typed,
        [this](uint32_t i) -> const std::string & { return phrases_[i].text; },
        limit,
        [&](uint32_t i) { return phrases_[i].text.size() > typed.size(); },
        [this](uint32_t i) {
            return std::make_pair(phrases_[i].freq, &phrases_[i].text);
        });
}

std::vector<std::string> PhraseIndex::expand(const std::string &initials,
                                             size_t limit,
                                             size_t minSyllables) const {
    if (!AllBopomofo(initials) || CodePointCount(initials) < minSyllables ||
        phrases_.empty())
        return {};
    sortIfNeeded();
    // Exact abbreviation only: ㄍㄒㄈㄘ is four syllables, not a prefix.
    return Collect(
        byAbbr_, initials,
        [this](uint32_t i) -> const std::string & { return phrases_[i].abbr; },
        limit, [&](uint32_t i) { return phrases_[i].abbr == initials; },
        [this](uint32_t i) {
            return std::make_pair(phrases_[i].freq, &phrases_[i].text);
        });
}

} // namespace chewing_plus
