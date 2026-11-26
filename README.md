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
# 若系統已有官方 libchewing / fcitx5-chewing，或舊的 *-plus 套件，不要用
# `pacman -U --noconfirm`（衝突提示的預設是 N，升級會被略過）。請用：
sudo packaging/release/install-arch.sh chewing-plus-fcitx5-*.pkg.tar.zst
# 不要手動 `pacman -Rdd libchewing fcitx5-chewing …`：若已安裝
# chewing-plus-*，那些名稱只是它的 provides，卸載會失敗。本機不用腳本時，
# 請用沒有 --noconfirm 的 `pacman -U`，衝突提示答 y。
# IBus：
sudo packaging/release/install-arch.sh chewing-plus-ibus-*.pkg.tar.zst
# 裝完執行 ibus restart，在 IBus 設定加入「新酷音 Plus」
```

從套件庫 `pacman -Syu` 升級時，PKGBUILD 的 `replaces=` 會自動換成新酷音 Plus（預設答 Y）。

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

### 候選視窗與提示字大小（Linux）

候選字浮動視窗、組字／提示文字，以及語音狀態（例如「🎙 聆聽中」）都是由輸入法前端的介面繪製，不是新酷音 Plus 自己的視窗。專案裡沒有另外的字級設定，請直接調整前端的字型：

**fcitx5：** 設定 → 附加元件 → 經典使用者介面 → 字型（預設 `Sans 10`）。也可以編輯 `~/.config/fcitx5/conf/classicui.conf`：

```
Font="Sans 13"
```

存檔後執行 `fcitx5-remote -r`，或重啟 fcitx5。若在 Wayland（例如 Hyprland）把字型調大後仍偏小，可在同一檔案把 `ForceWaylandDPI` 設成 `144` 或 `192`（`0` 表示不強制，沿用 96 DPI 再乘上螢幕縮放）。`EnableFractionalScale` 預設已開啟。`MenuFont` 只影響系統列選單，不會改候選視窗。

**IBus：** IBus 偏好設定 → 通用 → 字型與風格 → 勾選「使用自訂字型」，再選較大的字型。未勾選時沿用系統 GTK 字型；自訂預設是 `Sans 10`。也可以：

```
gsettings set org.freedesktop.ibus.panel use-custom-font true
gsettings set org.freedesktop.ibus.panel custom-font 'Sans 13'
```

然後執行 `ibus restart`。

若應用程式把組字區嵌在文字框裡，那段文字的大小由該程式決定，不受上述設定影響；浮動區塊與提示字仍走前端介面。

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

- **按住快捷鍵** 說話，放開後輸出文字（Linux 預設右 Alt，macOS 預設右 Option；fcitx5、IBus、macOS 都可以自訂）
- 辨識結果會插入目前游標的位置，並轉成繁體中文
- 會自動參考你自己加的詞，提高專有名詞的辨識率

語音**預設不啟用**，因為模型載入後語音服務約佔 260 MB 記憶體。要用時：

| 平台 | 啟用方式 |
|---|---|
| fcitx5 | 設定 → 新酷音 → 勾選 `Voice: enable dictation` |
| IBus | 「新酷音 Plus」設定程式 → 「啟用語音」 |
| macOS | 安裝時在「自訂」畫面勾選「啟用語音」，或之後從輸入法選單開啟 |

第一次使用前要下載語音模型（約 850 MB），在「語音模型」視窗操作（語音辨識與翻譯分兩區，可下載、續傳、取消、刪除、檢查、切換）：

| 平台 | 怎麼開 |
|---|---|
| fcitx5 | 輸入法選單（狀態區）的「語音模型…」、設定 → 新酷音 → `Voice: model manager` 按鈕，或應用程式選單的「新酷音 Plus 語音模型」 |
| IBus | 「新酷音 Plus」設定程式 →「語音模型…」，或「新酷音 Plus 語音模型」 |
| macOS | 輸入法選單「語音模型…」，或 /Applications 的「新酷音 Plus 語音模型」 |

#### 語音翻譯（實驗）
按住**翻譯快捷鍵**說中文，放開後輸入翻譯後的文字（預設翻成英文）。翻譯也在本機進行，
用的是 [HY-MT1.5-1.8B](https://huggingface.co/tencent/HY-MT1.5-1.8B-GGUF)（約 1.1 GB，
Tencent HY Community License，不適用於歐盟、英國與南韓）。使用前請在「語音模型…」
同意授權並下載；未同意時會先送出原文。

翻譯**預設關閉**。Arch 要另外安裝翻譯外掛 `chewing-plus-translate`；macOS 的安裝檔已內含外掛：

| 平台 | 開啟 | 快捷鍵 | 目標語言 |
|---|---|---|---|
| fcitx5 | 設定 → 附加元件 → Voxkit → `Voice translation` | 右 Ctrl（`TranslateHotkey`） | `TranslateTarget` |
| IBus | 「新酷音 Plus」設定程式 → 「語音翻譯」 | 設定程式的「語音翻譯快捷鍵」（預設右 Ctrl，`ibus.conf` 的 `voice_translate_key`） | `~/.config/chewing-plus/ibus.conf` 的 `voice_translate_to` |
| macOS | 輸入法選單 →「語音翻譯」 | 右 ⌘（「快速鍵…」可改） | 輸入法選單 →「語音翻譯成」 |

翻譯失敗、模型還在下載或沒有外掛時，會改輸入原本的辨識結果並顯示原因。

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
