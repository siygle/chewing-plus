/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
// Read-only Plus dictionaries shared by every session in the process.
//
// macOS opens one session per text field and IBus one per client; each used
// to load its own copy of the English (~15 MB), phrase (~10 MB) and emoji
// data. DataCache builds each dictionary once per set of files and hands out
// shared_ptr<const T>. Entries are finalized (sorted) before they are shared,
// so const lookups never write and sessions can query them concurrently.
// When the last session drops a dictionary it is freed.
#ifndef SRC_PLUSDATA_H_
#define SRC_PLUSDATA_H_

#include <memory>
#include <string>

#include "PlusEmoji.h"
#include "PlusEnglish.h"
#include "PlusPhrases.h"

namespace chewing_plus {

namespace DataCache {
// Built-in file first, then the user file (either may be "" or missing).
// Returns an empty dictionary rather than null when nothing loads.
std::shared_ptr<const EnglishLexicon> english(const std::string &builtIn,
                                              const std::string &user);
std::shared_ptr<const EmojiShortcodes> emoji(const std::string &builtIn,
                                             const std::string &user);
std::shared_ptr<const PhraseIndex> phrases(const std::string &path);
// Number of dictionaries currently alive, for tests.
size_t liveCount();
} // namespace DataCache

} // namespace chewing_plus
#endif
