// SPDX-License-Identifier: MIT
#include "PlusEnglish.h"

#include <algorithm>
#include <fstream>
#include <unordered_set>

namespace chewing_plus {
namespace {
constexpr size_t kMaxWord = 40;
constexpr size_t kMaxWords = 300000;

std::string Lower(std::string_view s) {
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return r;
}

bool AllLower(const std::string &s) {
    return std::none_of(s.begin(), s.end(),
                        [](unsigned char c) { return std::isupper(c); });
}
} // namespace

std::string_view EnglishLexicon::keyOf(const Entry &e) const {
    return {pool_.data() + e.key, e.len};
}

std::string_view EnglishLexicon::textOf(const Entry &e) const {
    return {pool_.data() + e.text, e.len};
}

// Sorted by key; within a key, user casings first, then load order. That is
// the casing order the old map kept (user entries moved to the front).
void EnglishLexicon::sortIfNeeded() const {
    if (sorted_)
        return;
    std::sort(entries_.begin(), entries_.end(),
              [this](const Entry &a, const Entry &b) {
                  auto ka = keyOf(a);
                  auto kb = keyOf(b);
                  if (ka != kb)
                      return ka < kb;
                  if (a.user != b.user)
                      return a.user > b.user;
                  // User casings: the latest load first.
                  return a.user ? a.order > b.order : a.order < b.order;
              });
    sorted_ = true;
}

void EnglishLexicon::finalize() { sortIfNeeded(); }

void EnglishLexicon::clear() {
    pool_.clear();
    entries_.clear();
    sorted_ = true;
    count_ = 0;
    nextOrder_ = 0;
}

std::pair<const EnglishLexicon::Entry *, const EnglishLexicon::Entry *>
EnglishLexicon::range(std::string_view key) const {
    sortIfNeeded();
    const Entry *first = entries_.data();
    const Entry *last = first + entries_.size();
    auto lo = std::lower_bound(
        first, last, key,
        [this](const Entry &e, std::string_view k) { return keyOf(e) < k; });
    auto hi = lo;
    while (hi != last && keyOf(*hi) == key)
        ++hi;
    return {lo, hi};
}

size_t EnglishLexicon::load(const std::string &path, bool userFile) {
    // Read the whole file once and scan it in place: no per-line strings,
    // so loading leaves no freed heap behind (that alone was ~13 MB of
    // resident memory for 70k words).
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return 0;
    std::string file((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    sortIfNeeded();

    // Duplicate check against what is already loaded: binary search in the
    // sorted entries (by key, then compare casings), no extra set.
    auto has = [this](std::string_view word) {
        auto [lo, hi] = range(Lower(word));
        for (auto it = lo; it != hi; ++it) {
            if (textOf(*it) == word)
                return true;
        }
        return false;
    };
    const size_t before = entries_.size();
    std::vector<Entry> added;
    size_t n = 0;
    size_t pos = 0;
    while (pos < file.size() && count_ + n < kMaxWords) {
        size_t eol = file.find('\n', pos);
        if (eol == std::string::npos)
            eol = file.size();
        std::string_view line(file.data() + pos, eol - pos);
        pos = eol + 1;
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        // Trim surrounding whitespace for hand-edited user files.
        auto b = line.find_first_not_of(" \t");
        if (b == std::string_view::npos || line[b] == '#')
            continue;
        auto e = line.find_last_not_of(" \t");
        std::string_view w = line.substr(b, e - b + 1);
        if (w.empty() || w.size() > kMaxWord ||
            !std::all_of(w.begin(), w.end(),
                         [](unsigned char c) { return c > 0x20 && c < 0x7f; }))
            continue;
        ++n;
        if (before && has(w)) {
            if (userFile) { // Promote an existing casing to user.
                for (size_t i = 0; i < before; i++) {
                    auto &x = entries_[i];
                    if (!x.user && textOf(x) == w) {
                        x.user = 1;
                        x.order = nextOrder_++;
                        sorted_ = false;
                    }
                }
            }
            continue;
        }
        Entry entry{};
        entry.len = static_cast<uint16_t>(w.size());
        entry.user = userFile ? 1 : 0;
        entry.order = nextOrder_++;
        entry.key = static_cast<uint32_t>(pool_.size());
        for (unsigned char c : w)
            pool_.push_back(static_cast<char>(std::tolower(c)));
        entry.text = static_cast<uint32_t>(pool_.size());
        pool_.append(w);
        added.push_back(entry);
    }
    if (!added.empty()) {
        entries_.insert(entries_.end(), added.begin(), added.end());
        sorted_ = false;
    }
    std::vector<Entry>().swap(added);
    std::string().swap(file);
    // Same casing twice in this file: keep the first.
    sortIfNeeded();
    entries_.erase(std::unique(entries_.begin(), entries_.end(),
                               [this](const Entry &a, const Entry &b) {
                                   return keyOf(a) == keyOf(b) &&
                                          textOf(a) == textOf(b);
                               }),
                   entries_.end());
    count_ += n;
    pool_.shrink_to_fit();
    entries_.shrink_to_fit();
    return n;
}

bool EnglishLexicon::contains(const std::string &word) const {
    if (word.empty())
        return false;
    auto [lo, hi] = range(Lower(word));
    return lo != hi;
}

std::string EnglishLexicon::canonical(const std::string &typed) const {
    if (typed.empty())
        return typed;
    auto [lo, hi] = range(Lower(typed));
    if (lo == hi)
        return typed;
    if (lo->user)
        return std::string(textOf(*lo));
    // Only auto-correct when the user did not choose a casing themselves.
    if (!AllLower(typed))
        return typed;
    // "apple"/"Apple" both exist: lowercase input is already correct.
    for (auto it = lo; it != hi; ++it) {
        if (textOf(*it) == typed)
            return typed;
    }
    return hi - lo == 1 ? std::string(textOf(*lo)) : typed;
}

std::vector<std::string> EnglishLexicon::candidates(const std::string &typed,
                                                    size_t limit) const {
    std::vector<std::string> out;
    if (typed.empty() || !limit)
        return out;
    std::unordered_set<std::string> seen;
    auto add = [&](std::string_view s) {
        std::string str(s);
        if (out.size() < limit && seen.insert(str).second)
            out.push_back(std::move(str));
    };
    add(canonical(typed));
    add(typed);
    const auto key = Lower(typed);
    auto [lo, hi] = range(key);
    for (auto it = lo; it != hi; ++it)
        add(textOf(*it));
    if (out.size() >= limit)
        return out;

    // Completions (longer words with this prefix), user words first. One
    // representative casing per key: the first in casing order.
    std::vector<const Entry *> user, builtIn;
    const Entry *last = entries_.data() + entries_.size();
    std::string_view prev;
    for (auto it = hi; it != last && keyOf(*it).starts_with(key); ++it) {
        if (keyOf(*it) == prev)
            continue;
        prev = keyOf(*it);
        (it->user ? user : builtIn).push_back(it);
    }
    auto byLength = [this](const Entry *a, const Entry *b) {
        if (a->len != b->len)
            return a->len < b->len;
        return textOf(*a) < textOf(*b);
    };
    std::stable_sort(user.begin(), user.end(), byLength);
    std::stable_sort(builtIn.begin(), builtIn.end(), byLength);
    for (auto *e : user)
        add(textOf(*e));
    for (auto *e : builtIn)
        add(textOf(*e));
    return out;
}

} // namespace chewing_plus
