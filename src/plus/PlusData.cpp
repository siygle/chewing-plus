/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "PlusData.h"

#include <map>
#include <mutex>
#include <sys/stat.h>

namespace chewing_plus {
namespace {

// A user file edited on disk must not keep serving the old copy, so the key
// includes each file's size and modification time.
std::string Stamp(const std::string &path) {
    if (path.empty())
        return "-";
    struct stat st{};
    if (stat(path.c_str(), &st) != 0)
        return path + "|missing";
    return path + "|" + std::to_string(static_cast<long long>(st.st_size)) +
           "|" + std::to_string(static_cast<long long>(st.st_mtime));
}

template <typename T>
using Table = std::map<std::string, std::weak_ptr<const T>>;

std::mutex &Lock() {
    static std::mutex m;
    return m;
}

template <typename T, typename Build>
std::shared_ptr<const T> Get(Table<T> &table, const std::string &key,
                             Build build) {
    std::lock_guard<std::mutex> guard(Lock());
    if (auto it = table.find(key); it != table.end()) {
        if (auto alive = it->second.lock())
            return alive;
    }
    // Drop entries whose last user is gone.
    for (auto it = table.begin(); it != table.end();) {
        it = it->second.expired() ? table.erase(it) : std::next(it);
    }
    auto made = std::make_shared<T>();
    build(*made);
    std::shared_ptr<const T> shared = made;
    table[key] = shared;
    return shared;
}

Table<EnglishLexicon> &EnglishTable() {
    static Table<EnglishLexicon> t;
    return t;
}
Table<EmojiShortcodes> &EmojiTable() {
    static Table<EmojiShortcodes> t;
    return t;
}
Table<PhraseIndex> &PhraseTable() {
    static Table<PhraseIndex> t;
    return t;
}
std::map<std::string, std::weak_ptr<UserHistory>> &HistoryTable() {
    static std::map<std::string, std::weak_ptr<UserHistory>> t;
    return t;
}

template <typename T>
size_t Live(Table<T> &table) {
    size_t n = 0;
    for (auto &[key, weak] : table)
        n += !weak.expired();
    return n;
}

} // namespace

namespace DataCache {

std::shared_ptr<const EnglishLexicon> english(const std::string &builtIn,
                                              const std::string &user) {
    return Get(EnglishTable(), Stamp(builtIn) + "\n" + Stamp(user),
               [&](EnglishLexicon &lex) {
                   if (!builtIn.empty())
                       lex.load(builtIn);
                   if (!user.empty())
                       lex.load(user, true);
                   lex.finalize();
               });
}

std::shared_ptr<const EmojiShortcodes> emoji(const std::string &builtIn,
                                             const std::string &user) {
    return Get(EmojiTable(), Stamp(builtIn) + "\n" + Stamp(user),
               [&](EmojiShortcodes &codes) {
                   if (!builtIn.empty())
                       codes.load(builtIn);
                   if (!user.empty())
                       codes.load(user, true);
               });
}

std::shared_ptr<const PhraseIndex> phrases(const std::string &path) {
    return Get(PhraseTable(), Stamp(path), [&](PhraseIndex &index) {
        if (!path.empty())
            index.load(path);
        index.finalize();
    });
}

std::shared_ptr<const PhraseIndex> userPhrases(const std::string &path) {
    return Get(PhraseTable(), "user\n" + Stamp(path), [&](PhraseIndex &index) {
        if (!path.empty())
            index.load(path, false, true);
        index.finalize();
    });
}

std::shared_ptr<const PhraseIndex> chars(const std::string &path) {
    // Same table, distinct key: a text file loads differently here.
    return Get(PhraseTable(), "chars\n" + Stamp(path), [&](PhraseIndex &index) {
        if (!path.empty())
            index.load(path, true);
        index.finalize();
    });
}

std::shared_ptr<UserHistory> userHistory(const std::string &path) {
    if (path.empty()) {
        return std::make_shared<UserHistory>();
    }
    std::lock_guard<std::mutex> guard(Lock());
    auto &table = HistoryTable();
    if (auto it = table.find(path); it != table.end()) {
        if (auto alive = it->second.lock()) {
            return alive;
        }
    }
    for (auto it = table.begin(); it != table.end();) {
        it = it->second.expired() ? table.erase(it) : std::next(it);
    }
    auto made = std::make_shared<UserHistory>();
    made->load(path);
    table[path] = made;
    return made;
}

size_t liveCount() {
    std::lock_guard<std::mutex> guard(Lock());
    size_t hist = 0;
    for (auto &[key, weak] : HistoryTable())
        hist += !weak.expired();
    return Live(EnglishTable()) + Live(EmojiTable()) + Live(PhraseTable()) +
           hist;
}

} // namespace DataCache
} // namespace chewing_plus
