/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "PlusPhrases.h"
#include "PlusUserHistory.h"

#include <algorithm>
#include <cmath>
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
constexpr uint32_t kDefaultUserPhraseFreq = 500;

size_t CodePointCount(std::string_view s) {
    return std::count_if(s.begin(), s.end(),
                         [](unsigned char c) { return (c & 0xC0) != 0x80; });
}

bool StartsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

// Extract the first Bopomofo symbol (U+3105..U+3129) of each space-separated
// syllable, or return `raw` as-is if it is already compact Bopomofo initials.
std::string NormalizeAbbr(std::string_view raw) {
    if (raw.find(' ') == std::string_view::npos) {
        return std::string(raw);
    }
    std::string out;
    size_t i = 0;
    while (i < raw.size()) {
        while (i < raw.size() && raw[i] == ' ') {
            ++i;
        }
        if (i >= raw.size()) {
            break;
        }
        size_t end = raw.find(' ', i);
        if (end == std::string_view::npos) {
            end = raw.size();
        }
        if (end - i >= 3) {
            std::string first(raw.substr(i, 3));
            if (AllBopomofo(first)) {
                out += first;
            }
        }
        i = end;
    }
    return out;
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

size_t PhraseIndex::load(const std::string &path, bool readings,
                         bool userFile) {
    if (loadCompiled(path)) {
        return size();
    }
    std::ifstream in(path);
    if (!in)
        return 0;
    size_t added = 0;
    std::string line;
    while (std::getline(in, line) && phrases_.size() < kMaxPhrases) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        auto t1 = line.find('\t');
        auto t2 = t1 == std::string::npos ? t1 : line.find('\t', t1 + 1);
        Phrase p;
        if (t1 == std::string::npos) {
            if (!userFile || readings) {
                continue;
            }
            p.text = line;
            p.freq = kDefaultUserPhraseFreq;
            p.abbr.clear();
        } else if (t2 == std::string::npos) {
            if (!userFile || readings) {
                continue;
            }
            p.text = line.substr(0, t1);
            p.freq = kDefaultUserPhraseFreq;
            p.abbr = NormalizeAbbr(std::string_view(line).substr(t1 + 1));
        } else {
            p.text = line.substr(0, t1);
            std::string_view rawAbbr = std::string_view(line).substr(t2 + 1);
            p.abbr = userFile && !readings ? NormalizeAbbr(rawAbbr)
                                           : std::string(rawAbbr);
            try {
                p.freq = static_cast<uint32_t>(
                    std::stoul(line.substr(t1 + 1, t2 - t1 - 1)));
            } catch (...) {
                continue;
            }
        }
        if (p.text.empty() || p.text.size() > kMaxPhraseBytes)
            continue;
        if (p.abbr.empty()) {
            if (!userFile || readings) {
                continue;
            }
        } else if (!AllBopomofo(p.abbr) ||
                   (!readings &&
                    CodePointCount(p.abbr) != CodePointCount(p.text))) {
            continue;
        }
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

bool PhraseIndex::containsText(std::string_view phrase) const {
    if (phrase.empty() || size() == 0) {
        return false;
    }
    sortIfNeeded();
    const uint32_t *idx = order(false);
    const size_t n = size();
    const uint32_t *lo = std::lower_bound(
        idx, idx + n, phrase,
        [this](uint32_t i, std::string_view k) { return text(i) < k; });
    return lo != idx + n && text(*lo) == phrase;
}

std::vector<std::string> PhraseIndex::collect(
    bool byAbbr, std::string_view key, size_t limit,
    const std::function<bool(const PhraseIndex &, uint32_t)> &accept,
    const PhraseIndex *extra, const UserHistory *history, bool isChar,
    int64_t nowSec, const std::vector<std::string> *exclude) const {
    if (limit == 0) {
        return {};
    }
    const bool useHistory = history != nullptr && !history->empty();
    const auto kind =
        isChar ? UserHistory::Kind::Char : UserHistory::Kind::Phrase;

    struct Hit {
        std::string text;
        double score = 0.0;
    };
    auto better = [](const Hit &a, const Hit &b) {
        if (std::abs(a.score - b.score) > 1e-9) {
            return a.score > b.score;
        }
        if (a.text.size() != b.text.size()) {
            return a.text.size() < b.text.size();
        }
        return a.text < b.text;
    };
    auto betterRaw = [](double scA, std::string_view txtA, const Hit &b) {
        if (std::abs(scA - b.score) > 1e-9) {
            return scA > b.score;
        }
        if (txtA.size() != b.text.size()) {
            return txtA.size() < b.text.size();
        }
        return txtA < b.text;
    };

    std::vector<Hit> heap;
    heap.reserve(limit);

    auto consider = [&](std::string_view sv, double sc) {
        if (exclude &&
            std::find(exclude->begin(), exclude->end(), sv) != exclude->end()) {
            return;
        }
        for (auto &h : heap) {
            if (h.text == sv) {
                if (sc > h.score + 1e-9) {
                    h.score = sc;
                    std::make_heap(heap.begin(), heap.end(), better);
                }
                return;
            }
        }
        if (heap.size() < limit) {
            heap.push_back({std::string(sv), sc});
            std::push_heap(heap.begin(), heap.end(), better);
        } else if (betterRaw(sc, sv, heap.front())) {
            std::pop_heap(heap.begin(), heap.end(), better);
            heap.back().text.assign(sv);
            heap.back().score = sc;
            std::push_heap(heap.begin(), heap.end(), better);
        }
    };

    auto scanIndex = [&](const PhraseIndex &src) {
        if (src.size() == 0) {
            return;
        }
        src.sortIfNeeded();
        const uint32_t *idx = src.order(byAbbr);
        const size_t n = src.size();
        auto keyOf = [&](uint32_t i) {
            return byAbbr ? src.abbr(i) : src.text(i);
        };
        const uint32_t *lo = std::lower_bound(
            idx, idx + n, key,
            [&](uint32_t i, std::string_view k) { return keyOf(i) < k; });
        for (const uint32_t *it = lo;
             it != idx + n && StartsWith(keyOf(*it), key); ++it) {
            if (!accept(src, *it)) {
                continue;
            }
            std::string_view sv = src.text(*it);
            uint32_t f = src.freq(*it);
            double sc = useHistory ? history->score(kind, sv, f, nowSec)
                                   : static_cast<double>(f);
            consider(sv, sc);
        }
    };

    scanIndex(*this);
    if (extra && extra->size() > 0) {
        scanIndex(*extra);
    }
    if (useHistory) {
        std::vector<UserHistory::ExtraItem> histItems;
        if (!byAbbr && !isChar) {
            histItems = history->matchCompletions(key, nowSec);
        } else if (byAbbr && !isChar) {
            histItems = history->matchExpansions(key, nowSec);
        }
        for (const auto &item : histItems) {
            consider(item.text, item.score);
        }
    }

    std::sort(heap.begin(), heap.end(), better);
    std::vector<std::string> out;
    out.reserve(heap.size());
    for (auto &h : heap) {
        out.push_back(std::move(h.text));
    }
    return out;
}

std::vector<std::string> PhraseIndex::complete(const std::string &typed,
                                               size_t limit, size_t minChars,
                                               const PhraseIndex *extra,
                                               const UserHistory *history,
                                               int64_t nowSec) const {
    if (CodePointCount(typed) < minChars) {
        return {};
    }
    if (size() == 0 && (!extra || extra->size() == 0) &&
        (!history || history->empty())) {
        return {};
    }
    return collect(
        false, typed, limit,
        [&](const PhraseIndex &src, uint32_t i) {
            return src.text(i).size() > typed.size();
        },
        extra, history, false, nowSec);
}

std::vector<std::string> PhraseIndex::byReading(const std::string &typed,
                                                size_t limit,
                                                const PhraseIndex *extra,
                                                const UserHistory *history,
                                                int64_t nowSec) const {
    if (!AllBopomofo(typed)) {
        return {};
    }
    if (size() == 0 && (!extra || extra->size() == 0)) {
        return {};
    }
    // Characters read exactly as typed come first (ㄋㄧ: 你 before 年
    // ㄋㄧㄢ), then longer readings that start with it (ㄋ: ㄋㄧ, ㄋㄢ...).
    std::vector<std::string> out = collect(
        true, typed, limit,
        [&](const PhraseIndex &src, uint32_t i) {
            return src.abbr(i) == typed;
        },
        extra, history, true, nowSec);
    if (out.size() >= limit) {
        return out;
    }
    std::vector<std::string> longer = collect(
        true, typed, limit - out.size(),
        [&](const PhraseIndex &src, uint32_t i) {
            return src.abbr(i) != typed;
        },
        extra, history, true, nowSec, &out);
    for (auto &s : longer) {
        if (out.size() >= limit) {
            break;
        }
        out.push_back(std::move(s));
    }
    return out;
}

std::vector<std::string> PhraseIndex::expand(const std::string &initials,
                                             size_t limit, size_t minSyllables,
                                             const PhraseIndex *extra,
                                             const UserHistory *history,
                                             int64_t nowSec) const {
    if (!AllBopomofo(initials) || CodePointCount(initials) < minSyllables) {
        return {};
    }
    if (size() == 0 && (!extra || extra->size() == 0) &&
        (!history || history->empty())) {
        return {};
    }
    // Exact abbreviation only: ㄍㄒㄈㄘ is four syllables, not a prefix.
    return collect(
        true, initials, limit,
        [&](const PhraseIndex &src, uint32_t i) {
            return src.abbr(i) == initials;
        },
        extra, history, false, nowSec);
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
