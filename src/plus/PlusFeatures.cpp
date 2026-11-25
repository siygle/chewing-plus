// SPDX-License-Identifier: MIT
#include "PlusFeatures.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace chewing_plus {
namespace {
// Split UTF-8 into code points. Input comes from the engine (valid UTF-8);
// a stray byte becomes its own element rather than being dropped.
std::vector<std::string> Split(const std::string &s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
        auto b = static_cast<unsigned char>(s[i]);
        size_t n = b < 0x80         ? 1
                   : (b >> 5) == 6  ? 2
                   : (b >> 4) == 14 ? 3
                   : (b >> 3) == 30 ? 4
                                    : 1;
        if (i + n > s.size())
            n = 1;
        out.push_back(s.substr(i, n));
        i += n;
    }
    return out;
}
class Calculator {
public:
    explicit Calculator(const std::string &input) : input_(input) {}
    double run() {
        double result = sum(0);
        space();
        if (pos_ != input_.size() || !std::isfinite(result))
            throw 0;
        return result;
    }

private:
    void space() {
        while (pos_ < input_.size() && input_[pos_] == ' ')
            ++pos_;
    }
    bool take(char c) {
        space();
        if (pos_ < input_.size() && input_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }
    double sum(int depth) {
        double v = product(depth);
        while (true) {
            if (take('+'))
                v += product(depth);
            else if (take('-'))
                v -= product(depth);
            else
                return v;
            if (!std::isfinite(v))
                throw 0;
        }
    }
    double product(int depth) {
        double v = atom(depth);
        while (true) {
            if (take('*'))
                v *= atom(depth);
            else if (take('/')) {
                double divisor = atom(depth);
                if (divisor == 0)
                    throw 0;
                v /= divisor;
            } else if (take('%')) {
                double divisor = atom(depth);
                if (divisor == 0)
                    throw 0;
                v = std::fmod(v, divisor);
            } else
                return v;
            if (!std::isfinite(v))
                throw 0;
        }
    }
    double atom(int depth) {
        if (depth > 32)
            throw 0;
        if (take('+'))
            return atom(depth + 1);
        if (take('-'))
            return -atom(depth + 1);
        if (take('(')) {
            double v = sum(depth + 1);
            if (!take(')'))
                throw 0;
            return v;
        }
        space();
        size_t start = pos_;
        bool digit = false;
        while (pos_ < input_.size() && input_[pos_] >= '0' &&
               input_[pos_] <= '9') {
            digit = true;
            ++pos_;
        }
        if (pos_ < input_.size() && input_[pos_] == '.') {
            ++pos_;
            while (pos_ < input_.size() && input_[pos_] >= '0' &&
                   input_[pos_] <= '9') {
                digit = true;
                ++pos_;
            }
        }
        if (!digit)
            throw 0;
        std::istringstream stream(input_.substr(start, pos_ - start));
        stream.imbue(std::locale::classic());
        double v;
        if (!(stream >> v) || !std::isfinite(v))
            throw 0;
        return v;
    }
    const std::string &input_;
    size_t pos_ = 0;
};
// Decode a single, already split UTF-8 scalar. Input is engine-generated UTF-8.
uint32_t Scalar(const std::string &c) {
    if (c.empty())
        return 0;
    auto b = static_cast<unsigned char>(c[0]);
    if (b < 128)
        return b;
    uint32_t v = b & (c.size() == 2 ? 31 : c.size() == 3 ? 15 : 7);
    for (size_t i = 1; i < c.size(); ++i)
        v = (v << 6) | (c[i] & 63);
    return v;
}
bool Han(const std::string &c) {
    uint32_t v = Scalar(c);
    return (v >= 0x3400 && v <= 0x9fff) || (v >= 0xf900 && v <= 0xfaff) ||
           (v >= 0x20000 && v <= 0x323af);
}
// CJK letters that English should be set off from: Han, Bopomofo (and
// extended), kana. Punctuation such as 。，」 is deliberately left out: it
// already carries its own spacing.
bool CjkLetter(const std::string &c) {
    if (Han(c))
        return true;
    uint32_t v = Scalar(c);
    return (v >= 0x3105 && v <= 0x312f) || (v >= 0x31a0 && v <= 0x31bf) ||
           (v >= 0x3041 && v <= 0x3096) || (v >= 0x30a1 && v <= 0x30fa) ||
           (v >= 0x31f0 && v <= 0x31ff);
}
bool Ascii(const std::string &c, bool letters, bool digits) {
    if (c.size() != 1)
        return false;
    return (letters &&
            ((c[0] >= 'a' && c[0] <= 'z') || (c[0] >= 'A' && c[0] <= 'Z'))) ||
           (digits && c[0] >= '0' && c[0] <= '9');
}
} // namespace
std::optional<std::string> Calculate(const std::string &expression) {
    if (expression.empty() || expression.size() > 256)
        return std::nullopt;
    try {
        double result = Calculator(expression).run();
        if (result == 0)
            result = 0; // No negative zero.
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(12) << result;
        return out.str();
    } catch (...) {
        return std::nullopt;
    }
}
std::string AutoSpace(const std::string &text, bool letters, bool digits) {
    const auto chars = Split(text);
    std::string result;
    for (size_t i = 0; i < chars.size(); ++i) {
        if (i && ((Han(chars[i - 1]) && Ascii(chars[i], letters, digits)) ||
                  (Ascii(chars[i - 1], letters, digits) && Han(chars[i]))))
            result += ' ';
        result += chars[i];
    }
    return result;
}
bool NeedsSpaceBeforeEnglish(const std::string &preceding) {
    if (preceding.empty())
        return false;
    // Start of the last UTF-8 sequence.
    size_t i = preceding.size() - 1;
    while (i > 0 && (static_cast<unsigned char>(preceding[i]) & 0xC0) == 0x80)
        --i;
    const std::string last = preceding.substr(i);
    return last.size() >= 2 && last.size() <= 4 && CjkLetter(last);
}
std::vector<std::string> EmojiForText(const std::string &text) {
    // Small curated Traditional Chinese defaults; no private/proprietary data.
    static const std::vector<std::pair<std::string, std::string>> entries = {
        {"開心", "😊"}, {"高興", "😊"}, {"哈哈", "😂"}, {"笑死", "🤣"},
        {"難過", "😢"}, {"傷心", "😭"}, {"生氣", "😠"}, {"無言", "😑"},
        {"尷尬", "😅"}, {"害羞", "☺️"},  {"謝謝", "🙏"}, {"感謝", "🙏"},
        {"讚", "👍"},   {"加油", "💪"}, {"恭喜", "🎉"}, {"生日快樂", "🎂"},
        {"愛你", "❤️"},  {"早安", "☀️"},  {"晚安", "🌙"}, {"咖啡", "☕"},
        {"台灣", "🇹🇼"}, {"臺灣", "🇹🇼"}, {"火鍋", "🍲"}};
    std::vector<std::string> result;
    for (const auto &[word, emoji] : entries) {
        if (text.ends_with(word))
            result.push_back(text + emoji);
    }
    return result;
}
} // namespace chewing_plus
