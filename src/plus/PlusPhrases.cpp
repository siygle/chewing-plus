/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "PlusPhrases.h"

#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace chewing_plus {
namespace {
constexpr size_t kMaxPhrases = 400000;
constexpr size_t kMaxPhraseBytes = 120;
constexpr char kMagic[8] = {'C', 'P', 'P', 'H', 'R', 'S', '0', '1'};

size_t CodePointCount(std::string_view s) {
    return std::count_if(s.begin(), s.end(),
                         [](unsigned char c) { return (c & 0xC0) != 0x80; });
}

bool StartsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

/*
 * Compiled file, little-endian, every offset from the start of the file:
 *   0   magic "CPPHRS01"
 *   8   u32 phrase count N
 *   12  u32 offset of the record table
 *   16  u32 offset of the by-text index
 *   20  u32 offset of the by-abbreviation index
 *   24  u32 offset of the string pool
 *   28  u32 string pool size
 *   records: N x {u32 text_off, u16 text_len, u16 abbr_len, u32 abbr_off,
 *                 u32 freq}   (16 bytes, offsets into the pool)
 *   by-text: N x u32 record index, sorted by text
 *   by-abbr: N x u32 record index, sorted by abbreviation
 */
struct Header {
    char magic[8];
    uint32_t count;
    uint32_t records;
    uint32_t byText;
    uint32_t byAbbr;
    uint32_t pool;
    uint32_t poolSize;
};
struct Record {
    uint32_t textOff;
    uint16_t textLen;
    uint16_t abbrLen;
    uint32_t abbrOff;
    uint32_t freq;
};
static_assert(sizeof(Header) == 32, "header layout");
static_assert(sizeof(Record) == 16, "record layout");

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

PhraseIndex::~PhraseIndex() { unmap(); }

void PhraseIndex::unmap() {
    if (map_) {
        munmap(const_cast<void *>(map_), mapSize_);
    }
    map_ = nullptr;
    mapSize_ = 0;
}

void PhraseIndex::clear() {
    unmap();
    phrases_.clear();
    byText_.clear();
    byAbbr_.clear();
    sorted_ = true;
}

size_t PhraseIndex::size() const {
    return map_ ? static_cast<const Header *>(map_)->count : phrases_.size();
}

size_t PhraseIndex::load(const std::string &path) {
    if (loadCompiled(path)) {
        return size();
    }
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

// Map a compiled file read-only. The pages are shared with every other
// process that maps the same file, and nothing is parsed or sorted here.
bool PhraseIndex::loadCompiled(const std::string &path) {
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    struct stat st{};
    char magic[8] = {};
    bool ok = fstat(fd, &st) == 0 && st.st_size >= (off_t)sizeof(Header) &&
              pread(fd, magic, sizeof(magic), 0) == (ssize_t)sizeof(magic) &&
              std::memcmp(magic, kMagic, sizeof(kMagic)) == 0;
    void *map = nullptr;
    if (ok) {
        map = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ,
                   MAP_SHARED, fd, 0);
        ok = map != MAP_FAILED;
    }
    close(fd);
    if (!ok)
        return false;
    // Validate every offset once, so lookups never read past the map.
    const auto size = static_cast<uint64_t>(st.st_size);
    const auto *h = static_cast<const Header *>(map);
    const uint64_t n = h->count;
    auto inside = [size](uint64_t off, uint64_t len) {
        return off <= size && len <= size - off;
    };
    ok = n <= kMaxPhrases && inside(h->records, n * sizeof(Record)) &&
         inside(h->byText, n * 4) && inside(h->byAbbr, n * 4) &&
         inside(h->pool, h->poolSize) && h->records % 4 == 0 &&
         h->byText % 4 == 0 && h->byAbbr % 4 == 0;
    const auto *rec = reinterpret_cast<const Record *>(
        static_cast<const char *>(map) + h->records);
    const auto *bt = reinterpret_cast<const uint32_t *>(
        static_cast<const char *>(map) + h->byText);
    const auto *ba = reinterpret_cast<const uint32_t *>(
        static_cast<const char *>(map) + h->byAbbr);
    for (uint64_t i = 0; ok && i < n; i++) {
        ok = inside(rec[i].textOff, rec[i].textLen) &&
             rec[i].textOff + uint64_t(rec[i].textLen) <= h->poolSize &&
             rec[i].abbrOff + uint64_t(rec[i].abbrLen) <= h->poolSize &&
             bt[i] < n && ba[i] < n;
    }
    if (!ok) {
        munmap(map, static_cast<size_t>(st.st_size));
        return false;
    }
    clear();
    map_ = map;
    mapSize_ = static_cast<size_t>(st.st_size);
    return true;
}

std::string_view PhraseIndex::text(uint32_t i) const {
    if (!map_)
        return phrases_[i].text;
    const auto *h = static_cast<const Header *>(map_);
    const auto *base = static_cast<const char *>(map_);
    const auto &r = reinterpret_cast<const Record *>(base + h->records)[i];
    return {base + h->pool + r.textOff, r.textLen};
}

std::string_view PhraseIndex::abbr(uint32_t i) const {
    if (!map_)
        return phrases_[i].abbr;
    const auto *h = static_cast<const Header *>(map_);
    const auto *base = static_cast<const char *>(map_);
    const auto &r = reinterpret_cast<const Record *>(base + h->records)[i];
    return {base + h->pool + r.abbrOff, r.abbrLen};
}

uint32_t PhraseIndex::freq(uint32_t i) const {
    if (!map_)
        return phrases_[i].freq;
    const auto *h = static_cast<const Header *>(map_);
    const auto *base = static_cast<const char *>(map_);
    return reinterpret_cast<const Record *>(base + h->records)[i].freq;
}

const uint32_t *PhraseIndex::order(bool byAbbr) const {
    if (!map_)
        return byAbbr ? byAbbr_.data() : byText_.data();
    const auto *h = static_cast<const Header *>(map_);
    return reinterpret_cast<const uint32_t *>(static_cast<const char *>(map_) +
                                              (byAbbr ? h->byAbbr : h->byText));
}

void PhraseIndex::sortIfNeeded() const {
    if (sorted_ || map_)
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

std::vector<std::string>
PhraseIndex::collect(bool byAbbr, std::string_view key, size_t limit,
                     const std::function<bool(uint32_t)> &accept) const {
    sortIfNeeded();
    const uint32_t *idx = order(byAbbr);
    const size_t n = size();
    auto keyOf = [&](uint32_t i) { return byAbbr ? abbr(i) : text(i); };
    const uint32_t *lo = std::lower_bound(
        idx, idx + n, key,
        [&](uint32_t i, std::string_view k) { return keyOf(i) < k; });
    std::vector<uint32_t> hits;
    for (const uint32_t *it = lo; it != idx + n && StartsWith(keyOf(*it), key);
         ++it) {
        if (accept(*it)) {
            hits.push_back(*it);
        }
    }
    std::sort(hits.begin(), hits.end(), [&](uint32_t a, uint32_t b) {
        if (freq(a) != freq(b))
            return freq(a) > freq(b);
        if (text(a).size() != text(b).size())
            return text(a).size() < text(b).size();
        return text(a) < text(b);
    });
    std::vector<std::string> out;
    for (size_t i = 0; i < hits.size() && out.size() < limit; ++i) {
        out.emplace_back(text(hits[i]));
    }
    return out;
}

std::vector<std::string> PhraseIndex::complete(const std::string &typed,
                                               size_t limit,
                                               size_t minChars) const {
    if (CodePointCount(typed) < minChars || size() == 0)
        return {};
    return collect(false, typed, limit,
                   [&](uint32_t i) { return text(i).size() > typed.size(); });
}

std::vector<std::string> PhraseIndex::expand(const std::string &initials,
                                             size_t limit,
                                             size_t minSyllables) const {
    if (!AllBopomofo(initials) || CodePointCount(initials) < minSyllables ||
        size() == 0)
        return {};
    // Exact abbreviation only: ㄍㄒㄈㄘ is four syllables, not a prefix.
    return collect(true, initials, limit,
                   [&](uint32_t i) { return abbr(i) == initials; });
}

bool PhraseIndex::writeCompiled(const std::string &path) const {
    sortIfNeeded();
    if (map_)
        return false; // Already compiled; nothing to rebuild from.
    const uint32_t n = static_cast<uint32_t>(phrases_.size());
    std::string pool;
    std::vector<Record> records(n);
    for (uint32_t i = 0; i < n; i++) {
        const auto &p = phrases_[i];
        records[i].textOff = static_cast<uint32_t>(pool.size());
        records[i].textLen = static_cast<uint16_t>(p.text.size());
        pool += p.text;
        records[i].abbrOff = static_cast<uint32_t>(pool.size());
        records[i].abbrLen = static_cast<uint16_t>(p.abbr.size());
        pool += p.abbr;
        records[i].freq = p.freq;
    }
    Header h{};
    std::memcpy(h.magic, kMagic, sizeof(kMagic));
    h.count = n;
    h.records = sizeof(Header);
    h.byText = h.records + n * sizeof(Record);
    h.byAbbr = h.byText + n * 4;
    h.pool = h.byAbbr + n * 4;
    h.poolSize = static_cast<uint32_t>(pool.size());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(&h), sizeof(h));
    out.write(reinterpret_cast<const char *>(records.data()),
              static_cast<std::streamsize>(records.size() * sizeof(Record)));
    out.write(reinterpret_cast<const char *>(byText_.data()),
              static_cast<std::streamsize>(byText_.size() * 4));
    out.write(reinterpret_cast<const char *>(byAbbr_.data()),
              static_cast<std::streamsize>(byAbbr_.size() * 4));
    out.write(pool.data(), static_cast<std::streamsize>(pool.size()));
    return static_cast<bool>(out);
}

} // namespace chewing_plus
