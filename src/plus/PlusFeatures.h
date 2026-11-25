// SPDX-License-Identifier: MIT
#ifndef SRC_PLUSFEATURES_H_
#define SRC_PLUSFEATURES_H_

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace chewing_plus {
// No shell, interpreter, network, or locale-dependent parsing.
std::optional<std::string> Calculate(const std::string &expression);
std::string AutoSpace(const std::string &text, bool letters, bool digits);
// True when `preceding` ends with a Chinese (Han, Bopomofo) or Japanese kana
// character, i.e. English inserted right after it needs a separating space.
// Whitespace, ASCII, punctuation (full width too), digits and empty text say
// no. Only the last code point is looked at.
bool NeedsSpaceBeforeEnglish(const std::string &preceding);
std::vector<std::string> EmojiForText(const std::string &text);

} // namespace chewing_plus
#endif
