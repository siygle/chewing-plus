(function () {
  "use strict";

  const I18N = {
    "zh-tw": {
      _title: "新酷音 Plus — 免打聲調、動態建議排序、內嵌英文與本機語音翻譯的注音輸入法",
      _desc:
        "新酷音輸入法加強版：免打聲調、邊打邊列字、動態建議排序、154K+ 中文詞庫與 91K+ 英文辨識字典、Emoji 短碼、計算機與本機語音輸入／翻譯，支援 macOS、fcitx5 與 IBus。",
      brand: "新酷音 Plus",
      "nav.demo": "互動體驗",
      "nav.features": "特色功能",
      "nav.arch": "架構與效能",
      "nav.download": "下載安裝",
      "hero.badge": "v6.2.0 · 基於 libchewing 0.14",
      "hero.title": '為現代打字節奏打造的<span class="accent-text">新酷音 Plus</span>',
      "hero.subtitle":
        "免打聲調、邊打邊列字、動態建議排序、154K+ 詞庫與 91K+ 英文智慧辨識、Emoji 短碼、即時計算機，以及 100% 離線本機語音輸入與翻譯。一份核心，橫跨 fcitx5、IBus 與 macOS。",
      "hero.ctaDownload": "下載安裝檔",
      "hero.ctaGithub": "檢視原始碼",
      "demo.tab.toneless": "免打聲調",
      "demo.tab.initials": "字首注音",
      "demo.tab.english": "英文模式",
      "demo.tab.emoji": "Emoji 短碼",
      "demo.tab.calc": "計算機",
      "demo.tab.voice": "本機語音",
      "demo.tab.translate": "語音翻譯",
      "demo.replay": "重播",
      "features.eyebrow": "PLUS 功能一覽",
      "features.title": "保留新酷音的準確，加上超順手的現代效率工具",
      "features.desc":
        "所有 Plus 功能皆建構在公開的 libchewing C API 之上，不破壞原本注音習慣，並可逐項開關。",
      "f.toneless.badge": "全平台預設開啟",
      "f.toneless.title": "免打聲調",
      "f.toneless.desc":
        "不必逐字敲聲調鍵——輸入下一個字的聲母時，前一個字會自動選好。照原本習慣打聲調也完全相容。",
      "f.live.badge": "全平台預設開啟",
      "f.live.title": "邊打邊列字",
      "f.live.desc":
        "每敲一個注音符號，下方立刻列出最可能的常用字，用 <kbd>Shift</kbd>+<kbd>1…9</kbd> 直接選取，不必等整個音節打完。",
      "f.suggestions.badge": "154,000+ 內建詞彙",
      "f.suggestions.title": "詞語建議與字首注音",
      "f.suggestions.desc":
        "整合 libchewing、麥克注音（MIT）與現代台灣詞彙，提供超過 154,000 筆詞條。打前兩個字自動補全後文，或只敲每個字的第一個注音直接展開整句，用 <kbd>Shift</kbd>+<kbd>1…9</kbd> 選取，觸發門檻可自訂（預設 2 個聲母）。",
      "f.adaptive.badge": "v6.2 新增",
      "f.adaptive.title": "動態建議排序與自訂詞庫",
      "f.adaptive.desc":
        "依個人使用頻率與近期紀錄（Frecency）自動調整詞語建議、字首展開與邊打邊列字排序，自動匯入 <code>libchewing</code> 使用者詞庫，並支援 <code>plus-phrases-user.txt</code> 熱重載覆蓋。",
      "f.adaptive.r1": "越常用的詞自動排前面",
      "f.english.badge": "91,000+ 英文詞彙",
      "f.english.title": "內嵌英文模式與智慧提示",
      "f.english.desc":
        "內建超過 91,000 筆英文詞彙（SCOWL/ESDB 60 + 現代科技、AI、常見品牌與台灣地名／企業）。免切輸入法即可在中文裡插入英文並自動修正大小寫與補空白；忘記切模式直接打英文時，下方自動提示一鍵轉換。",
      "f.english.or": "或",
      "f.english.rshift": "右 Shift ×2",
      "f.english.hintNote": "直接把英文當注音打也會自動偵測",
      "f.emoji.title": "Emoji 候選與 1,900+ 短碼",
      "f.emoji.desc":
        "打「開心」按 <kbd>↓</kbd> 直接帶出「開心😊」；連按兩下 <kbd>;</kbd> 就能用 GitHub / Slack 風格短碼搜尋超過 1,900 個表情符號，並支援自訂詞表。",
      "f.calc.title": "即時計算機",
      "f.calc.desc":
        '組字區為空時按 <kbd>=</kbd> 直接輸入四則算式，按 <kbd>Enter</kbd> 輸出結果。支援 <code class="inline-code">+ - * / % ( )</code> 與小數。',
      "f.spacing.title": "中英數自動空格",
      "f.spacing.desc":
        "中文與英文字母、數字相接時自動補上盤古空白，英文字母與數字可獨立開關，排版乾淨不費力。",
      "f.voice.badge": "選用啟用 · 100% 離線",
      "f.voice.title": "本機語音輸入與翻譯（voxkit）",
      "f.voice.desc":
        "按住聽寫鍵說話即出字（Linux 預設右 Alt，macOS 預設右 Option），按住翻譯鍵則透過本機 HY-MT1.5 模型直接翻成英文送出（Linux 預設右 Ctrl，macOS 預設右 Cmd，皆可自訂）。三平台皆以 Qwen3-ASR-0.6B + Silero VAD + OpenCC <code>s2twp</code> 離線辨識，並自動帶入你的新酷音個人詞庫。",
      "f.voice.p1": "🔒 音訊與翻譯不離機",
      "f.voice.p2": "🇹🇼 OpenCC 台灣用語轉換",
      "f.voice.p3": "📚 全平台參考個人詞庫",
      "f.voice.p5": "🌐 本機 HY-MT1.5 語音翻譯",
      "f.models.badge": "macOS + Linux 原生介面",
      "f.models.title": "語音模型管理視窗",
      "f.models.desc":
        "macOS 採原生 SwiftUI、Linux 採 GTK4 / libadwaita，分成「語音辨識模型」與「翻譯模型」兩區，支援下載、續傳、取消、驗證、切換與刪除，服務未啟用時也能離線管理。",
      "f.privacy.title": "密碼欄位防護與中／英切換",
      "f.privacy.lshift": "左 Shift",
      "f.privacy.desc":
        "偵測到密碼或敏感欄位時，自動停用所有 Plus 面板、動態學習紀錄、自動空格與語音輸入。IBus 與 macOS 內建單按左 <kbd>Shift</kbd> 切換中／英並於狀態列顯示指示。",
      "arch.eyebrow": "架構設計",
      "arch.title": "一份核心，三個原生平台，零框架包袱",
      "arch.desc":
        "所有按鍵處理、設定表與記憶體映射詞庫都集中在 C++20 核心與穩定 C ABI，每個平台只負責繪製原生介面。",
      "arch.libchewing": "未修改上游引擎 · Bigram 語言模型 · v4 詞庫",
      "arch.core": "C++20 核心 · 穩定 C ABI · Frecency 動態排序 · mmap 154K+ 詞庫 & 91K+ 英文字典",
      "arch.fcitx5": "原生 C++ Addon + GTK4 模型視窗",
      "arch.ibus": "純 C 引擎 + libadwaita 設定與模型視窗",
      "arch.macos": "原生 Swift / InputMethodKit + SwiftUI",
      "arch.voxkit": "選用本機語音與翻譯服務 · Qwen3-ASR-0.6B · Silero VAD · HY-MT1.5 · OpenCC s2twp",
      "metric.mmap.title": "預編譯 mmap 詞庫載入",
      "metric.mmap.desc":
        "154,000+ 中文建議詞條與 18,000+ 單字表預先編譯成二進位索引，啟動零解析且跨程序共用記憶體分頁。",
      "metric.mem.title": "macOS 全功能執行記憶體",
      "metric.mem.desc":
        "純 AppKit + InputMethodKit 原生實作，無 Electron、無 Qt，多視窗共用同一份詞庫快取。",
      "metric.single.title": "每平台單一安裝檔",
      "metric.single.desc":
        "內含 libchewing 0.14、輸入法本體、詞庫與語音服務，下載一次即可完成安裝。",
      "dl.eyebrow": "下載與安裝",
      "dl.title": "每個平台，只要一個安裝檔",
      "dl.desc":
        "所有安裝檔均附帶 libchewing 0.14、輸入法、預編譯詞庫與選用的 voxkit 語音服務。",
      "dl.macos.sub": "Apple Silicon · macOS 13+",
      "dl.macos.desc":
        "安裝後自動註冊「新酷音 Plus」並附帶語音翻譯外掛與「語音模型」管理視窗。所有 Plus 功能預設開啟，語音輸入可在安裝程式「自訂」畫面或輸入法選單中啟用。",
      "dl.macos.gatekeeper":
        "套件未經 Apple 簽章：請對 .pkg 按右鍵 →「打開」以通過 Gatekeeper。",
      "dl.macos.btn": "下載 macOS 安裝檔",
      "dl.fcitx5.desc":
        "取代系統的 libchewing（0.13）與 fcitx5-chewing，內附 GTK4「語音模型」管理視窗。免打聲調與邊打邊列字預設開啟，另可加裝選用的 <code>chewing-plus-translate</code> 語音翻譯外掛。",
      "dl.ibus.desc":
        "含原生 IBus 引擎與「新酷音 Plus」設定程式（可管理 Plus 開關與語音模型）。安裝後執行 ibus restart 並加入輸入法即可。",
      "dl.copy": "複製",
      "dl.copied": "已複製",
      "dl.releases": "前往 GitHub Releases",
      "dl.defaults.title": "預設行為與自訂詞庫",
      "dl.defaults.body":
        "免打聲調、邊打邊列字與動態建議排序在三個平台皆預設開啟；語音輸入預設不啟用（載入辨識模型約佔 260 MB RAM，可在「語音模型」視窗下載）。個人自訂檔統一放於 Linux <code>~/.local/share/chewing-plus/</code>（相容舊路徑）或 macOS <code>~/Library/Application Support/ChewingPlus/</code>，支援 <code>plus-phrases-user.txt</code>、<code>plus-english.txt</code> 與 <code>plus-emoji-shortcodes.txt</code>。",
      "footer.releases": "版本下載",
      "footer.dev": "開發文件",
      "footer.issues": "問題回報",
      "footer.credits":
        "基於 libchewing 0.14 與 fcitx5-chewing；內建資料來自 libchewing-data、McBopomofo（MIT）、gemoji（MIT）、SCOWL/ESDB（MIT 類）與 fcitx5-voxkit（MIT）。",
    },
    en: {
      _title: "Chewing Plus — Modern Traditional Chinese Bopomofo Input Method",
      _desc:
        "Enhanced Chewing (新酷音) Bopomofo input method: toneless typing, live character bar, adaptive frecency ranking, 154K+ phrases, 91K+ inline English words, emoji shortcodes, calculator, and offline voice dictation & translation for macOS, fcitx5, and IBus.",
      brand: "Chewing Plus",
      "nav.demo": "Playground",
      "nav.features": "Features",
      "nav.arch": "Architecture",
      "nav.download": "Download",
      "hero.badge": "v6.2.0 · Built on libchewing 0.14",
      "hero.title": 'Traditional Chinese typing, <span class="accent-text">reimagined</span>',
      "hero.subtitle":
        "Toneless Bopomofo, live character picks, adaptive frecency ranking, 154K+ phrases & 91K+ smart English words, emoji shortcodes, instant calculator, and 100% offline voice dictation & translation — one shared core across fcitx5, IBus, and macOS.",
      "hero.ctaDownload": "Download Installer",
      "hero.ctaGithub": "View on GitHub",
      "demo.tab.toneless": "Toneless",
      "demo.tab.initials": "Initials",
      "demo.tab.english": "Inline EN",
      "demo.tab.emoji": "Emoji",
      "demo.tab.calc": "Calculator",
      "demo.tab.voice": "Local Voice",
      "demo.tab.translate": "Voice Translate",
      "demo.replay": "Replay",
      "features.eyebrow": "WHAT'S IN PLUS",
      "features.title": "Chewing's accuracy, supercharged with modern productivity tools",
      "features.desc":
        "Every Plus feature sits cleanly on top of the public libchewing C API — never breaking classic Zhuyin muscle memory, and individually toggleable.",
      "f.toneless.badge": "On by default everywhere",
      "f.toneless.title": "Toneless Typing",
      "f.toneless.desc":
        "Skip tone keys whenever you want: typing the next syllable's initial automatically finishes the previous character. Classic toned typing keeps working seamlessly.",
      "f.live.badge": "On by default everywhere",
      "f.live.title": "Live Character Bar",
      "f.live.desc":
        "As soon as you press the first Bopomofo symbol, likely characters appear below. Pick any with <kbd>Shift</kbd>+<kbd>1…9</kbd> without finishing the syllable.",
      "f.suggestions.badge": "154,000+ Built-in Phrases",
      "f.suggestions.title": "Phrase Completion & Initials",
      "f.suggestions.desc":
        "Combines libchewing, McBopomofo (MIT), and curated modern Taiwan terms into 154,000+ phrases. Type two characters to complete or type only the first Bopomofo per syllable to expand with <kbd>Shift</kbd>+<kbd>1…9</kbd> (threshold configurable, default 2).",
      "f.adaptive.badge": "New in v6.2",
      "f.adaptive.title": "Adaptive Frecency & Custom Overlay",
      "f.adaptive.desc":
        "Re-ranks phrase completions, initials, and live characters by personal frequency & recency (Frecency). Automatically imports your <code>libchewing</code> user phrases and hot-reloads custom <code>plus-phrases-user.txt</code> overlays.",
      "f.adaptive.r1": "Frequently used words float to the top",
      "f.english.badge": "91,000+ English Words",
      "f.english.title": "Inline English Mode & Smart Hints",
      "f.english.desc":
        "Bundles 91,000+ English entries (SCOWL/ESDB size 60 + modern tech, AI, brand, and Taiwan terms). Insert auto-cased English directly into the buffer with automatic spacing; keep typing in Zhuyin and a live hint lets you convert with one shortcut.",
      "f.english.or": "or",
      "f.english.rshift": "Right Shift ×2",
      "f.english.hintNote": "Detects English words even if typed straight in Zhuyin",
      "f.emoji.title": "Emoji Candidates & 1,900+ Shortcodes",
      "f.emoji.desc":
        "Type a common phrase like 開心 and press <kbd>↓</kbd> for 開心😊, or double-tap <kbd>;</kbd> to search 1,900+ GitHub/Slack shortcodes with user-customizable overrides.",
      "f.calc.title": "Inline Calculator",
      "f.calc.desc":
        'Press <kbd>=</kbd> on an empty buffer, type any math expression, and hit <kbd>Enter</kbd> to commit the result. Supports <code class="inline-code">+ - * / % ( )</code> and decimals.',
      "f.spacing.title": "Automatic CJK–ASCII Spacing",
      "f.spacing.desc":
        "Automatically inserts clean spaces where Chinese meets English letters or digits. Letters and digits can be toggled independently.",
      "f.voice.badge": "Opt-in · 100% Offline",
      "f.voice.title": "On-Device Voice Dictation & Translation (voxkit)",
      "f.voice.desc":
        "Hold push-to-talk to dictate (Right Alt on Linux, Right Option on macOS), or hold the translate key to speak and insert an offline HY-MT1.5 translation (Right Ctrl on Linux, Right Cmd on macOS, customizable). Powered locally by Qwen3-ASR-0.6B, Silero VAD, and OpenCC <code>s2twp</code>, with your Chewing user phrases shared on all platforms.",
      "f.voice.p1": "🔒 Zero cloud upload",
      "f.voice.p2": "🇹🇼 Taiwan phrasing via OpenCC",
      "f.voice.p3": "📚 Learns from user phrases everywhere",
      "f.voice.p5": "🌐 Local HY-MT1.5 translation",
      "f.models.badge": "Native macOS + Linux UI",
      "f.models.title": "Voice Model Manager",
      "f.models.desc":
        "Native SwiftUI on macOS and GTK4/libadwaita on Linux with separate Speech and Translation sections — download, resume, cancel, verify, switch, and delete models even while the daemon is stopped.",
      "f.privacy.title": "Password Field Safety & 中/英 Toggle",
      "f.privacy.lshift": "Left Shift",
      "f.privacy.desc":
        "All Plus panels, adaptive usage recording, auto-spacing, and dictation strictly shut off in password and sensitive fields. Tap Left <kbd>Shift</kbd> to switch 中/英 on IBus and macOS with a status indicator.",
      "arch.eyebrow": "ARCHITECTURE",
      "arch.title": "One shared core, three native frontends, zero bloat",
      "arch.desc":
        "Key handling, unified settings, and memory-mapped dictionaries live inside a single C++20 core behind a stable C ABI. Each platform draws with its own native toolkit.",
      "arch.libchewing": "Unpatched upstream · Bigram LM · v4 user dictionary",
      "arch.core": "C++20 core · Stable C ABI · Frecency ranking · mmap 154K+ phrases & 91K+ English",
      "arch.fcitx5": "Native C++ Addon + GTK4 model window",
      "arch.ibus": "Pure C engine + libadwaita prefs & models",
      "arch.macos": "Native Swift / InputMethodKit + SwiftUI",
      "arch.voxkit": "Optional local voice & MT daemon · Qwen3-ASR-0.6B · Silero VAD · HY-MT1.5 · OpenCC s2twp",
      "metric.mmap.title": "Memory-mapped dictionary load",
      "metric.mmap.desc":
        "154,000+ phrase completions and 18,000+ live characters are pre-compiled into binary indices — zero load-time parsing, shared across processes.",
      "metric.mem.title": "Active memory on macOS",
      "metric.mem.desc":
        "Pure AppKit + InputMethodKit with no Qt or Electron. Dictionaries are cached and shared across every text field.",
      "metric.single.title": "Installer per platform",
      "metric.single.desc":
        "Bundles libchewing 0.14, the input method, compiled dictionaries, and voxkit in a single package.",
      "dl.eyebrow": "DOWNLOAD & INSTALL",
      "dl.title": "One installer per platform",
      "dl.desc":
        "Every package bundles libchewing 0.14, the input method, pre-compiled wordlists, and the optional voxkit voice daemon.",
      "dl.macos.sub": "Apple Silicon · macOS 13+",
      "dl.macos.desc":
        "Automatically registers Chewing Plus with the local translation plugin and SwiftUI Voice Models window. All Plus features start enabled; opt into voice during Installer customization or anytime from the input menu.",
      "dl.macos.gatekeeper":
        "Unsigned package: right-click the .pkg → Open to install past Gatekeeper.",
      "dl.macos.btn": "Download macOS .pkg",
      "dl.fcitx5.desc":
        "Replaces distro libchewing (0.13) and fcitx5-chewing in one transaction and includes the GTK4 Voice Models window. Add the optional <code>chewing-plus-translate</code> package for offline voice translation.",
      "dl.ibus.desc":
        "Includes the native IBus engine and GTK4/libadwaita setup app for Plus toggles and voice models. Run ibus restart and add Chewing Plus in IBus preferences.",
      "dl.copy": "Copy",
      "dl.copied": "Copied",
      "dl.releases": "Open GitHub Releases",
      "dl.defaults.title": "Defaults & custom wordlists",
      "dl.defaults.body":
        "Toneless typing, live characters, and adaptive suggestion ranking are enabled by default on all platforms. Voice dictation is opt-in (~260 MB RAM loaded; download models from the Voice Models window). Place custom <code>plus-phrases-user.txt</code>, <code>plus-english.txt</code>, or <code>plus-emoji-shortcodes.txt</code> in <code>~/.local/share/chewing-plus/</code> (Linux) or <code>~/Library/Application Support/ChewingPlus/</code> (macOS).",
      "footer.releases": "Releases",
      "footer.dev": "Development Docs",
      "footer.issues": "Issues",
      "footer.credits":
        "Built on libchewing 0.14 and fcitx5-chewing. Bundled data from libchewing-data, McBopomofo (MIT), gemoji (MIT), SCOWL/ESDB, and fcitx5-voxkit (MIT).",
    },
  };

  // Interactive IME demos
  const DEMOS = {
    toneless: {
      mode: "中",
      keys: ["ㄋ", "ㄧ", "ㄏ", "ㄠ", "ㄇ", "ㄚ", "Space", "Enter"],
      caption: {
        "zh-tw": "不用按聲調鍵，邊打邊列出候選字，打下一個聲母時自動轉字。",
        en: "No tone keys required — live characters appear as you type and convert automatically at the next initial.",
      },
      steps: [
        { k: 0, committed: "", preedit: "", hl: "ㄋ", aux: "", list: ["年", "您", "你", "能", "內"], prefix: "⇧", delay: 480 },
        { k: 1, committed: "", preedit: "", hl: "ㄋㄧ", aux: "", list: ["你", "妳", "泥", "擬", "逆"], prefix: "⇧", delay: 480 },
        { k: 2, committed: "", preedit: "你", hl: "ㄏ", aux: "", list: ["和", "好", "會", "很", "後"], prefix: "⇧", delay: 480 },
        { k: 3, committed: "", preedit: "你", hl: "ㄏㄠ", aux: "", list: ["好", "號", "毫", "耗"], prefix: "⇧", delay: 480 },
        { k: 4, committed: "", preedit: "你好", hl: "ㄇ", aux: "", list: ["們", "沒", "面", "嗎", "美"], prefix: "⇧", delay: 480 },
        { k: 5, committed: "", preedit: "你好", hl: "ㄇㄚ", aux: "", list: ["嗎", "媽", "馬", "碼"], prefix: "⇧", delay: 560 },
        { k: 6, committed: "", preedit: "你好嗎", hl: "", aux: "", list: [], delay: 620 },
        { k: 7, committed: "你好嗎", preedit: "", hl: "", aux: "", list: [], delay: 1800 },
      ],
    },
    initials: {
      mode: "中",
      keys: ["ㄍ", "ㄒ", "ㄈ", "ㄘ", "⇧1"],
      caption: {
        "zh-tw": "每個字只打第一個注音，Shift+1 直接展開（內建 154,000+ 詞條並依個人使用習慣動態排序）。",
        en: "Type only the first Bopomofo per syllable and press Shift+1 to expand (154,000+ phrases ranked adaptively by your usage).",
      },
      steps: [
        { k: 0, committed: "", preedit: "", hl: "ㄍ", aux: "", list: ["個", "國", "過", "高", "公"], prefix: "⇧", delay: 460 },
        { k: 1, committed: "", preedit: "", hl: "ㄍㄒ", aux: "", list: ["關係", "感謝", "高興", "更新", "公司"], prefix: "⇧", delay: 520 },
        { k: 2, committed: "", preedit: "", hl: "ㄍㄒㄈ", aux: "", list: ["恭喜發財"], prefix: "⇧", delay: 520 },
        { k: 3, committed: "", preedit: "", hl: "ㄍㄒㄈㄘ", aux: "", list: ["恭喜發財"], prefix: "⇧", selected: 0, delay: 720 },
        { k: 4, committed: "恭喜發財", preedit: "", hl: "", aux: "", list: [], delay: 1800 },
      ],
    },
    english: {
      mode: "中",
      keys: ["ㄋㄧˇ", "ㄏㄠˇ", "g", "i", "t", "h", "u", "b", "⌃⇧Space", "Enter"],
      caption: {
        "zh-tw": "內建 91,000+ 英文詞彙！不用切換輸入法，直接打英文時下方自動提示，一鍵轉換大小寫並補空白。",
        en: "91,000+ built-in English words: typing English straight in Zhuyin triggers a smart hint — one shortcut converts, auto-cases, and adds CJK spacing.",
      },
      steps: [
        { k: 0, committed: "", preedit: "你", hl: "", aux: "", list: [], delay: 420 },
        { k: 1, committed: "", preedit: "你好", hl: "", aux: "", list: [], delay: 480 },
        { k: 2, committed: "", preedit: "你好", hl: "ㄕ", aux: "", list: ["是", "時", "事"], prefix: "⇧", delay: 260 },
        { k: 3, committed: "", preedit: "你好", hl: "ㄛ", aux: "", list: [], delay: 260 },
        { k: 4, committed: "", preedit: "你好", hl: "ㄔ", aux: "", list: [], delay: 260 },
        { k: 5, committed: "", preedit: "你好", hl: "ㄘ", aux: "", list: [], delay: 260 },
        { k: 6, committed: "", preedit: "你好", hl: "ㄘㄧ", aux: "", list: [], delay: 260 },
        { k: 7, committed: "", preedit: "你好", hl: "ㄖ", aux: "Control+Shift+Space → GitHub", list: [], delay: 950 },
        { k: 8, committed: "", preedit: "你好GitHub ", hl: "", aux: "", list: [], delay: 650 },
        { k: 9, committed: "你好 GitHub ", preedit: "", hl: "", aux: "", list: [], delay: 1800 },
      ],
    },
    emoji: {
      mode: "中",
      keys: [";", ";", "f", "i", "r", "e", "Space"],
      caption: {
        "zh-tw": "快速連按兩下 ; 開啟 Emoji 短碼搜尋，內建 1,900+ GitHub / Slack 短碼，按空白鍵直接插入。",
        en: "Double-tap ; to open the 1,900+ gemoji shortcode picker, then press Space to insert right at your cursor.",
      },
      steps: [
        { k: 0, committed: "太強了 ", preedit: "", hl: "ㄤ", aux: "", list: [], delay: 340 },
        { k: 1, committed: "太強了 ", preedit: ";;", hl: "", aux: "", list: [], delay: 420 },
        { k: 2, committed: "太強了 ", preedit: ";;f", hl: "", aux: "", list: ["🔥 :fire", "🎉 :fireworks", "🚩 :flag"], prefix: "⇧", delay: 320 },
        { k: 3, committed: "太強了 ", preedit: ";;fi", hl: "", aux: "", list: ["🔥 :fire", "🎉 :fireworks", "🐟 :fish"], prefix: "⇧", delay: 320 },
        { k: 4, committed: "太強了 ", preedit: ";;fir", hl: "", aux: "", list: ["🔥 :fire", "🎉 :fireworks", "🚒 :fire_engine"], prefix: "⇧", delay: 320 },
        { k: 5, committed: "太強了 ", preedit: ";;fire", hl: "", aux: "", list: ["🔥 :fire", "🎉 :fireworks", "🚒 :fire_engine"], prefix: "⇧", selected: 0, delay: 760 },
        { k: 6, committed: "太強了 🔥", preedit: "", hl: "", aux: "", list: [], delay: 1800 },
      ],
    },
    calc: {
      mode: "中",
      keys: ["=", "1", "+", "2", "*", "3", "Enter"],
      caption: {
        "zh-tw": "組字區為空時按 = 啟動內建計算機，即時預覽運算結果，按 Enter 直接輸出數字。",
        en: "Press = on an empty buffer to evaluate math expressions live, then press Enter to commit the result.",
      },
      steps: [
        { k: 0, committed: "", preedit: "[計算器] =", hl: "", aux: "", list: [], delay: 420 },
        { k: 1, committed: "", preedit: "[計算器] =1", hl: "", aux: "", list: ["1"], prefix: "⇧", delay: 300 },
        { k: 2, committed: "", preedit: "[計算器] =1+", hl: "", aux: "", list: [], delay: 280 },
        { k: 3, committed: "", preedit: "[計算器] =1+2", hl: "", aux: "", list: ["3"], prefix: "⇧", delay: 320 },
        { k: 4, committed: "", preedit: "[計算器] =1+2*", hl: "", aux: "", list: [], delay: 280 },
        { k: 5, committed: "", preedit: "[計算器] =1+2*3", hl: "", aux: "", list: ["7"], prefix: "⇧", selected: 0, delay: 820 },
        { k: 6, committed: "7", preedit: "", hl: "", aux: "", list: [], delay: 1800 },
      ],
    },
    voice: {
      mode: "中",
      keys: ["按住右 ⌥ / Alt", "說話中…", "放開"],
      keysEn: ["Hold Right ⌥ / Alt", "Speak…", "Release"],
      caption: {
        "zh-tw": "本機 voxkit 服務（Qwen3-ASR-0.6B + OpenCC s2twp）離線辨識並自動套用台灣繁體與個人詞庫。",
        en: "Local voxkit daemon (Qwen3-ASR-0.6B + OpenCC s2twp) transcribes completely offline with your personal Chewing vocabulary.",
      },
      steps: [
        {
          k: 0,
          committed: "明天開會討論 ",
          preedit: "",
          hl: "",
          aux: "🎙 聆聽中… (100% local)",
          auxEn: "🎙 Listening… (100% local)",
          voice: true,
          list: [],
          delay: 950,
        },
        {
          k: 1,
          committed: "明天開會討論 ",
          preedit: "",
          hl: "",
          aux: "Qwen3-ASR-0.6B · OpenCC s2twp",
          auxEn: "Qwen3-ASR-0.6B · OpenCC s2twp",
          voice: true,
          list: [],
          delay: 900,
        },
        {
          k: 2,
          committed: "明天開會討論 新酷音 Plus 的設計規格",
          preedit: "",
          hl: "",
          aux: "",
          list: [],
          delay: 2000,
        },
      ],
    },
    translate: {
      mode: "中",
      keys: ["按住右 Ctrl / ⌘", "說中文…", "放開"],
      keysEn: ["Hold Right Ctrl / ⌘", "Speak Chinese…", "Release"],
      caption: {
        "zh-tw": "按住翻譯鍵直接說中文，透過本機 Qwen3-ASR + HY-MT1.5 離線翻譯成英文並送出至游標位置。",
        en: "Hold the translate key and speak Chinese — local Qwen3-ASR + HY-MT1.5 translates completely offline and commits English at your cursor.",
      },
      steps: [
        {
          k: 0,
          committed: "",
          preedit: "",
          hl: "",
          aux: "🎙 語音翻譯聆聽中… (100% local)",
          auxEn: "🎙 Listening for translation… (100% local)",
          voice: true,
          list: [],
          delay: 950,
        },
        {
          k: 1,
          committed: "",
          preedit: "",
          hl: "",
          aux: "HY-MT1.5 (llama.cpp) · 「請幫我把這個 PR 合併並發布新版」",
          auxEn: "HY-MT1.5 (llama.cpp) · Translating locally…",
          voice: true,
          list: [],
          delay: 1050,
        },
        {
          k: 2,
          committed: "Please merge this PR and publish a new release.",
          preedit: "",
          hl: "",
          aux: "",
          list: [],
          delay: 2100,
        },
      ],
    },
  };

  let currentLang = detectLang();
  let currentDemo = "toneless";
  let timer = null;

  function detectLang() {
    const params = new URLSearchParams(window.location.search);
    const q = (params.get("lang") || "").toLowerCase();
    if (q === "en" || q === "zh-tw") return q;
    try {
      const saved = localStorage.getItem("chewing-plus-lang");
      if (saved === "en" || saved === "zh-tw") return saved;
    } catch (_) {}
    const nav = (navigator.language || "").toLowerCase();
    return nav.startsWith("zh") ? "zh-tw" : "en";
  }

  function applyLang(lang, updateUrl) {
    currentLang = lang;
    const dict = I18N[lang] || I18N["zh-tw"];
    document.documentElement.lang = lang === "en" ? "en" : "zh-Hant";
    document.title = dict._title;
    const metaDesc = document.querySelector('meta[name="description"]');
    if (metaDesc) metaDesc.setAttribute("content", dict._desc);

    document.querySelectorAll("[data-i18n]").forEach((el) => {
      const key = el.getAttribute("data-i18n");
      if (dict[key] !== undefined) {
        el.innerHTML = dict[key];
      }
    });

    document.querySelectorAll(".lang-btn").forEach((btn) => {
      btn.setAttribute("aria-pressed", btn.getAttribute("data-lang") === lang ? "true" : "false");
    });

    const titleEl = document.getElementById("demo-window-title");
    if (titleEl) {
      titleEl.textContent = lang === "en" ? "Notes — Chewing Plus" : "備忘錄 — 新酷音 Plus";
    }

    try {
      localStorage.setItem("chewing-plus-lang", lang);
    } catch (_) {}

    if (updateUrl && window.history && window.history.replaceState) {
      const url = new URL(window.location.href);
      url.searchParams.set("lang", lang);
      window.history.replaceState({}, "", url.toString());
    }

    updateDemoCaption();
  }

  // Theme handling
  function initTheme() {
    let theme = null;
    try {
      theme = localStorage.getItem("chewing-plus-theme");
    } catch (_) {}
    if (!theme) {
      theme = window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches
        ? "dark"
        : "light";
    }
    document.documentElement.setAttribute("data-theme", theme);
  }

  function toggleTheme() {
    const cur = document.documentElement.getAttribute("data-theme") === "dark" ? "dark" : "light";
    const next = cur === "dark" ? "light" : "dark";
    document.documentElement.setAttribute("data-theme", next);
    try {
      localStorage.setItem("chewing-plus-theme", next);
    } catch (_) {}
  }

  // Playground animation engine
  const elCommitted = document.getElementById("demo-committed");
  const elPreeditWrap = document.getElementById("demo-preedit-wrap");
  const elPreedit = document.getElementById("demo-preedit");
  const elHl = document.getElementById("demo-hl");
  const elPopup = document.getElementById("demo-popup");
  const elAux = document.getElementById("demo-aux");
  const elList = document.getElementById("demo-list");
  const elKeys = document.getElementById("demo-keys");
  const elCaption = document.getElementById("demo-caption");
  const elModeBadge = document.getElementById("demo-mode-badge");

  function updateDemoCaption() {
    const demo = DEMOS[currentDemo];
    if (demo && elCaption) {
      elCaption.textContent = demo.caption[currentLang] || demo.caption["zh-tw"];
    }
  }

  function renderKeys(demo, activeIdx) {
    elKeys.innerHTML = "";
    const keys = currentLang === "en" && demo.keysEn ? demo.keysEn : demo.keys;
    keys.forEach((label, idx) => {
      const kbd = document.createElement("kbd");
      kbd.className = "demo-key";
      if (idx === activeIdx) kbd.classList.add("active");
      else if (idx < activeIdx) kbd.classList.add("done");
      kbd.textContent = label;
      elKeys.appendChild(kbd);
    });
  }

  function renderStep(demo, step) {
    elModeBadge.textContent = demo.mode;
    elCommitted.textContent = step.committed || "";
    elPreedit.textContent = step.preedit || "";
    elHl.textContent = step.hl || "";
    if (elPreeditWrap) {
      elPreeditWrap.hidden = !(step.preedit || step.hl);
    }

    const auxText = currentLang === "en" && step.auxEn ? step.auxEn : step.aux || "";
    const hasList = step.list && step.list.length > 0;

    if (!auxText && !hasList) {
      elPopup.classList.add("hidden");
      elAux.innerHTML = "";
      elList.innerHTML = "";
    } else {
      elPopup.classList.remove("hidden");
      if (step.voice) {
        elAux.innerHTML =
          '<span class="voice-wave" aria-hidden="true"><span></span><span></span><span></span><span></span><span></span></span>' +
          auxText;
      } else {
        elAux.textContent = auxText;
      }
      elList.innerHTML = "";
      if (hasList) {
        step.list.forEach((item, i) => {
          const btn = document.createElement("button");
          btn.type = "button";
          btn.className = "ime-item" + (step.selected === i ? " selected" : "");
          const keySpan = document.createElement("span");
          keySpan.className = "ime-item-key";
          keySpan.textContent = (step.prefix || "") + (i + 1);
          const textSpan = document.createElement("span");
          textSpan.textContent = item;
          btn.appendChild(keySpan);
          btn.appendChild(textSpan);
          // Let users click a candidate to commit immediately!
          btn.addEventListener("click", () => {
            clearTimeout(timer);
            const clean = item.includes(" :") ? item.split(" :")[0] : item;
            const finalStep = {
              k: demo.keys.length - 1,
              committed: (step.committed || "") + clean,
              preedit: "",
              hl: "",
              aux: "",
              list: [],
            };
            renderStep(demo, finalStep);
            renderKeys(demo, demo.keys.length);
          });
          elList.appendChild(btn);
        });
      }
    }

    renderKeys(demo, step.k);
  }

  function playDemo(name) {
    clearTimeout(timer);
    currentDemo = name;
    const demo = DEMOS[name];
    if (!demo) return;

    document.querySelectorAll(".demo-tab").forEach((tab) => {
      const active = tab.getAttribute("data-demo") === name;
      tab.classList.toggle("active", active);
      tab.setAttribute("aria-selected", active ? "true" : "false");
    });

    updateDemoCaption();

    let idx = 0;
    function tick() {
      const step = demo.steps[idx];
      renderStep(demo, step);
      idx = (idx + 1) % demo.steps.length;
      timer = setTimeout(tick, step.delay);
    }
    tick();
  }

  // Copy buttons
  function initCopyButtons() {
    document.querySelectorAll(".copy-btn").forEach((btn) => {
      btn.addEventListener("click", async () => {
        const text = btn.getAttribute("data-copy") || "";
        try {
          await navigator.clipboard.writeText(text);
        } catch (_) {
          const ta = document.createElement("textarea");
          ta.value = text;
          document.body.appendChild(ta);
          ta.select();
          document.execCommand("copy");
          document.body.removeChild(ta);
        }
        const dict = I18N[currentLang] || I18N["zh-tw"];
        btn.textContent = dict["dl.copied"];
        setTimeout(() => {
          btn.textContent = (I18N[currentLang] || I18N["zh-tw"])["dl.copy"];
        }, 1600);
      });
    });
  }

  // Wire up events
  initTheme();
  applyLang(currentLang, false);
  initCopyButtons();

  document.querySelectorAll(".lang-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      applyLang(btn.getAttribute("data-lang"), true);
      playDemo(currentDemo);
    });
  });

  const themeBtn = document.getElementById("theme-toggle");
  if (themeBtn) themeBtn.addEventListener("click", toggleTheme);

  document.querySelectorAll(".demo-tab").forEach((tab) => {
    tab.addEventListener("click", () => playDemo(tab.getAttribute("data-demo")));
  });

  const replayBtn = document.getElementById("demo-replay");
  if (replayBtn) replayBtn.addEventListener("click", () => playDemo(currentDemo));

  playDemo("toneless");
})();
