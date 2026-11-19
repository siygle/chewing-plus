/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The one list of Plus settings. Every frontend reads this table through
 * chewing_plus_setting_info() to build its settings UI and storage, and
 * applies values with chewing_plus_set_setting(). Add a setting here and
 * all of them get it.
 *
 * SETTING(key, type, default, label_en, label_zh)
 *   key       stable name used in config files ("emoji")
 *   type      BOOL, INT or STRING
 *   default   "1"/"0", a number, or a string
 *
 * Defaults are the core's; a frontend may ship different defaults for its
 * users (macOS turns the Plus features on) but must pass them explicitly.
 */
SETTING(emoji, BOOL, "0", "Emoji suggestions", "候選字顯示 Emoji")
SETTING(calculator, BOOL, "0", "Calculator (=)", "計算機（=）")
SETTING(space_letters, BOOL, "0",
        "Space between Chinese and ASCII letters", "中英文之間自動加空格")
SETTING(space_digits, BOOL, "0", "Space between Chinese and digits",
        "中文與數字之間自動加空格")
SETTING(shortcode, BOOL, "0", "Emoji shortcodes (;;smile)",
        "Emoji 短碼（;;）")
SETTING(shortcode_prefix, STRING, ";;", "Emoji shortcode prefix",
        "Emoji 短碼前綴")
SETTING(shortcode_tap_ms, INT, "300", "Shortcode double-tap window (ms)",
        "短碼連按間隔（毫秒）")
SETTING(english, BOOL, "0", "English mode", "英文模式")
SETTING(english_hint, BOOL, "1", "Hint when typed keys look like English",
        "打出英文單字時顯示提示")
SETTING(english_hint_commit, BOOL, "0",
        "English shortcut fills the hinted word",
        "出現英文提示時，英文快速鍵直接填入第一個")
SETTING(suggestions, BOOL, "0",
        "Phrase suggestions (completion and initials, Shift+1-9)",
        "詞語建議（自動完成、字首注音，⇧1–9 選取）")
SETTING(initials_min, INT, "2",
        "Initials needed before suggesting (2 or more)",
        "字首注音幾個以上開始建議")
SETTING(fuzzy, BOOL, "1",
        "Toneless typing: finish a syllable at the next initial",
        "免打聲調（打下一個聲母時自動選字）")
SETTING(live_chars, BOOL, "1",
        "List likely characters while a syllable is being typed",
        "邊打邊列出可能的字")
SETTING(shift_toggle, BOOL, "0", "Tap Left Shift alone to switch 中/英",
        "左 Shift 切換中／英")
SETTING(easy_symbol, BOOL, "0", "Easy symbol (Shift+letter)",
        "簡易符號（Shift＋英文字母）")
SETTING(keyboard_layout, STRING, "KB_DEFAULT", "Keyboard layout", "鍵盤排列")
SETTING(selection_keys, STRING, "1234567890", "Selection keys", "選字鍵")
SETTING(page_size, INT, "10", "Candidates per page", "每頁候選字數")
SETTING(space_as_selection, BOOL, "1", "Space opens the candidate list",
        "空白鍵選字")
SETTING(add_phrase_forward, BOOL, "1", "Add phrase forward", "向前加詞")
SETTING(choice_backward, BOOL, "1", "Backward phrase choice", "向後選詞")
SETTING(auto_shift_cursor, BOOL, "0", "Move cursor after selection",
        "選字後游標自動右移")
