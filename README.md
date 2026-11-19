# 新酷音 Plus（chewing-plus）

新酷音輸入法的加強版：免打聲調、邊打邊列字、詞語建議、emoji、計算機、英文模式與本機語音輸入。
支援 **fcitx5**、**IBus**（Linux）與 **macOS**，三個平台共用同一份核心
（`libchewing-plus-core`），功能一致。

基於 [libchewing](https://codeberg.org/chewing/libchewing) 0.14 與
[fcitx/fcitx5-chewing](https://github.com/fcitx/fcitx5-chewing)。

## 安裝

到 [Releases](https://github.com/siygle/chewing-plus/releases) 下載，每個平台只要一個安裝檔，
內含 libchewing 0.14、輸入法、詞庫與語音服務：

| 平台 | 檔案 |
|---|---|
| Arch Linux（fcitx5） | `chewing-plus-fcitx5-*.pkg.tar.zst` |
| Arch Linux（IBus） | `chewing-plus-ibus-*.pkg.tar.zst` |
| macOS（Apple Silicon） | `ChewingPlus-*-macos-arm64.pkg` |

```bash
sudo pacman -U chewing-plus-fcitx5-*.pkg.tar.zst   # fcitx5
sudo pacman -U chewing-plus-ibus-*.pkg.tar.zst     # IBus：裝完執行 ibus restart，在 IBus 設定加入「新酷音 Plus」
```

macOS 套件尚未經 Apple 簽章，請對 `.pkg` 按右鍵 → 打開。詳細步驟見各版本的發布說明。

## 設定

| 平台 | 在哪裡設定 |
|---|---|
| fcitx5 | fcitx5 設定 → 新酷音 → 設定（`Plus:` 開頭的選項） |
| IBus | 「新酷音 Plus」設定程式 |
| macOS | 選單列的輸入法選單；快速鍵在「快速鍵…」 |

**預設：** 免打聲調與邊打邊列字在所有平台都開啟。其他 Plus 功能在 macOS 預設開啟，
在 Linux 預設關閉。語音輸入在所有平台都預設不啟用。

在密碼等敏感欄位中，所有 Plus 功能都會自動停用。

## 功能

### 免打聲調
不用按聲調鍵：打下一個字的聲母時，前一個字會自動選好。例如 `ㄋㄧㄏㄠㄇㄚ` 加空白 → 「你好嗎」。
照樣打聲調也可以。

### 邊打邊列字
每打一個注音符號，下方就列出最可能的字，用 `Shift+1…9` 選取：

```
ㄋ   → 年 您 你 能 內 …
ㄋㄧ → 你 妳 泥 …
```

### 詞語建議
- **自動完成：** 打「中華」會建議「中華民國、中華隊…」
- **字首注音：** 每個字只打第一個注音，例如 `ㄍㄒㄈㄘ` → 「恭喜發財」、`ㄇㄇ` → 「媽媽」
- 用 `Shift+1…9` 選取，不影響原本的按鍵；幾個字首開始建議可以調整（預設 2）

### Emoji 候選
打出常見詞彙後按 `↓` 叫出候選，候選裡會出現對應的 emoji。
例如打「開心」，候選中會有「開心😊」。

### Emoji 短碼
快速連按兩下 `;`，再輸入短碼：

| 輸入 | 結果 |
|---|---|
| `;;smile` | 😄 |
| `;;fire` | 🔥 |
| `;;thumbsup` | 👍 |

- `空白`：輸入第一個候選
- `Shift+1…9`：選擇其他候選
- `Esc`：取消
- 內建 1,900 多個短碼，與 GitHub、Slack 的 `:shortcode:` 相同
- 前綴可改成其他按鍵，或改成單鍵觸發

### 計算機
組字區為空時按 `=`，輸入算式後按 `Enter`，就會輸出結果。
例如 `=1+2*3` → `7`。支援 `+ - * / % ( )` 與小數。

### 英文模式
不用切換輸入法就能打英文，而且會自動修正大小寫：

1. 按 `Ctrl+Shift+Space`，或連按兩下右 Shift，進入英文模式（組字區顯示 `[EN]`）
2. 輸入英文，例如 `github`
3. 按 `空白` 輸出 `GitHub`，並自動回到注音

| 按鍵 | 作用 |
|---|---|
| `空白` | 輸出修正大小寫的字，並加一個空格 |
| `Enter` | 照原樣輸出 |
| `Ctrl+1…9` | 選擇其他候選 |
| `Esc` | 取消英文，保留前面打好的中文 |

- 英文會插入目前游標的位置，前後的中文都會保留
- 數字與符號照常輸入，例如 `k8s`、`me@x.io`
- fcitx5 與 macOS 可以自訂快捷鍵

### 英文提示
不切換模式，直接把英文字當注音打，例如打 `github`。
當輸入法判斷這串按鍵像英文單字時，會在下方提示：

```
Ctrl+Shift+Space → GitHub
```

按快捷鍵就會轉成英文；不理會的話，就照原本的注音繼續輸入。
也可以設定成按快捷鍵直接填入提示的單字。

### 中／英切換
- **fcitx5：** 使用 fcitx5 內建的「暫時切換輸入法」鍵（預設單按左 Shift）
- **IBus、macOS：** 單按左 Shift 切換，IBus 面板與 macOS 選單列會顯示「中」／「英」

### 中英自動空格
中文與英文、數字相接時，會自動在中間加上空格：

```
你好Linux第5章  →  你好 Linux 第 5 章
```

英文字母與數字可以分別開關。

### 語音輸入
語音由內附的 [voxkit](https://github.com/siygle/fcitx5-voxkit) 服務在本機辨識，錄音不會上傳：

- **按住快捷鍵** 說話，放開後輸出文字（Linux 預設右 Alt，macOS 預設右 Option，可自訂）
- 辨識結果會插入目前游標的位置，並轉成繁體中文
- 會自動參考你自己加的詞，提高專有名詞的辨識率

語音**預設不啟用**，因為模型載入後語音服務約佔 260 MB 記憶體。要用時：

| 平台 | 啟用方式 |
|---|---|
| fcitx5 | 設定 → 新酷音 → 勾選 `Voice: enable dictation` |
| IBus | 「新酷音 Plus」設定程式 → 「啟用語音」 |
| macOS | 安裝時在「自訂」畫面勾選「啟用語音」，或之後從輸入法選單開啟 |

第一次使用前要下載語音模型（約 850 MB），可在設定中操作。

## 自訂詞庫（fcitx5）

在 `~/.local/share/fcitx5-chewing/` 建立以下檔案：

**`plus-emoji-shortcodes.txt`**：自訂 emoji 短碼，每行一個，會優先於內建短碼

```
tw 🇹🇼 taiwan
ok 👌
```

**`plus-english.txt`**：自訂英文單字與大小寫，每行一個，會優先於內建字典

```
OpenVanilla
k8s
```

修改後，切換一次輸入法即可生效。

## 授權

LGPL-2.1-or-later（與上游 fcitx5-chewing 相同）。

內建資料：
- 注音詞庫：[libchewing-data](https://codeberg.org/chewing/libchewing-data)（LGPL-2.1-or-later）
- Emoji 短碼：[gemoji](https://github.com/github/gemoji)（MIT）
- 英文字典：[SCOWL](http://wordlist.aspell.net/)（MIT 類授權）
- 語音服務 voxkit：MIT

建置與開發說明請見 [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)；各平台細節見
[docs/MACOS.md](docs/MACOS.md)、[docs/IBUS.md](docs/IBUS.md)、[docs/ABI.md](docs/ABI.md)。
