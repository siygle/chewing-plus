新酷音 Plus：在新酷音上加入 emoji、計算機、英文模式與本機語音輸入。基於 [fcitx/fcitx5-chewing](https://github.com/fcitx/fcitx5-chewing) 與 libchewing 0.14。功能說明請看 [README](https://github.com/siygle/chewing-plus#readme)。

## 6.0.3

- **修正 Omarchy 預設鍵盤選項下雙擊右 Shift 無效**：在 `shift:both_capslock_cancel` 下，右 Shift 放開時 keysym 會變成 Caps_Lock，快捷鍵對不到。改為同時以實體鍵碼辨識 Shift（右 Shift 62、左 Shift 50），單擊 Shift 與 Caps Lock 行為不變。
- **雙擊間隔改用按鍵事件時間戳**：先前以 fcitx5 讀到事件的到達時間計時，合成器忙碌、按鍵一次送達時，一次快速雙擊會被誤判成超時。改用合成器附在事件上的時間戳（32 位元溢位以環繞相減處理；前端未附時間戳則仍用到達時間）。
- **除錯日誌**：雙擊放開時以 `chewing` 類別記錄 hold、gap、window 與時間戳。fcitx5 5.1.23 請用 `fcitx5 --verbose=default=5,chewing=5`（`FCITX_LOG` 無效）。

## 6.0.2

- **英文快速插入前後都加空白**：在中文後面用英文快速插入（預設雙擊 Shift）時，英文前面也會補上空白（例如「你好 GitHub」），避免中英相接。前面已是空白、英文、數字或標點則不加，避免疊兩個空白。
- **升級內附 libchewing**：釘選上游 `v0.14.0-rc.2`（`1bfcde90`）。詞庫資料更新至 v2026.11.22，使用者詞庫／歷史紀錄內部實作調整，並新增選項 `chewing.keep_invalid_zhuyin`。
- **文件**：組建所需 Rust 最低版本更正為 1.95（與上游一致）。

## 6.0.1

- **修正免打聲調搭配使用者詞庫的選詞異常**：升級內附 `libchewing` 至上游最新 `v0.14.0-rc.1-17-gdad3241`（`dad32411`），修正開啟免打聲調時，若使用者詞庫（`user_dict.csv`）存有多字詞，輸入單一音節可能誤匹配成多字詞（例如 `ㄕㄜˋ` 直接變成「設計」）的問題。
- **IBus**：修復連按兩下右 Shift 切換英文模式。
- **文件與倉庫**：更名為 `chewing-plus`，補齊三平台說明。

## 6.0.0

新酷音 Plus 從這一版起獨立編號，不再跟著 fcitx5-chewing 的版號（之前是 5.1.13-plus.1～4）。從 5.1.13-plus.x 可直接升級。

**打字更順：免打聲調、邊打邊列字**（所有平台預設開啟）
- 免打聲調：打下一個聲母時自動選字，例如 ㄋㄧㄏㄠㄇㄚ 加空白 → 你好嗎。照樣打聲調也可以。
- 邊打邊列字：每打一個注音符號，浮動清單就列出最可能的字，例如 ㄋ → 年、您、你…；ㄋㄧ → 你、妳、泥…，用 Shift+1～9 選取。
- 字首注音和英文提示在免打聲調下也能正常使用（例如 你 + ㄍㄒㄈㄘ → 你恭喜發財）。

**詞語建議**
- 字首注音兩個就會比對，例如 ㄇㄇ → 密碼、買賣、媽媽…；門檻可在設定調整（預設 2）。

**三個平台用同一份核心**
- fcitx5、IBus、macOS 的按鍵處理、設定與詞庫都在共用核心；fcitx5 原本的 `chewing.conf` 設定全部保留。
- IBus 單按左 Shift 切換中／英（面板顯示「中」／「英」）；fcitx5 使用內建的「暫時切換輸入法」鍵。
- 所有設定由同一份清單定義，IBus 設定程式與 macOS 選單由清單自動產生。

**效能**
- 詞語建議與單字詞庫預先編譯並記憶體映射，載入不到 1 ms。
- 各輸入欄位共用詞庫；macOS 輸入法開啟所有 Plus 功能、正在輸入時約 30 MB。

**一個安裝檔**
- Linux 改成每個輸入框架一個安裝檔（`chewing-plus-fcitx5`、`chewing-plus-ibus`），跟 macOS 一樣裝一次就好；取代原本的四個套件。
- 語音改為預設不啟用，在設定（或 macOS 安裝程式／輸入法選單）勾選「啟用語音」才會啟動。

**之前的 5.1.13-plus.1～4**：macOS 可以安裝與正常打字（#7）、Plus 功能開關與快速鍵、語音繁體輸出、詞語建議、選單列「中／英」。

## 下載

| 平台 | 檔案 |
|---|---|
| Arch Linux（fcitx5） | `chewing-plus-fcitx5-*.pkg.tar.zst` |
| Arch Linux（IBus） | `chewing-plus-ibus-*.pkg.tar.zst` |
| macOS（Apple Silicon） | `ChewingPlus-*-macos-arm64.pkg` |

每個平台只要一個安裝檔，內含 libchewing 0.14、輸入法、詞庫與語音服務。`SHA256SUMS` 是所有檔案的校驗碼。

## 安裝：Arch Linux

依你用的輸入框架擇一安裝，會取代系統的 `libchewing`（0.13）與 `fcitx5-chewing`：

```bash
# 已有官方 libchewing / fcitx5-chewing 或舊的 *-plus 套件時，請用輔助腳本
# （`pacman -U --noconfirm` 對衝突提示會答 N，升級會被略過）：
sudo packaging/release/install-arch.sh chewing-plus-fcitx5-*.pkg.tar.zst
sudo packaging/release/install-arch.sh chewing-plus-ibus-*.pkg.tar.zst
# IBus 裝完執行 ibus restart，在 IBus 設定加入「新酷音 Plus」
```

從套件庫 `pacman -Syu` 升級時，`replaces=` 會自動取代官方 `libchewing` / `fcitx5-chewing` 以及舊的 `libchewing-plus`、`fcitx5-chewing-plus`、`ibus-chewing-plus`、`fcitx5-voxkit`（預設答 Y）。本機 `pacman -U` 仍會詢問，請答 `y`，或改用上面的腳本。兩個安裝檔互斥，切換框架時改裝另一個就好。

**語音輸入預設不啟用**（模型載入後語音服務約佔 260 MB 記憶體）。要用時：
- fcitx5：設定 → 新酷音 → 勾選 `Voice: enable dictation`
- IBus：在「新酷音 Plus」設定程式打開「啟用語音」
- 或在終端機執行 `systemctl --user enable --now voxkit.service`（關閉：`disable --now`）

第一次使用前要下載語音模型（約 850 MB），可在設定或 `/usr/libexec/chewing-plus-models menu` 操作。從舊版升級時，已啟用的語音維持啟用。

免打聲調與邊打邊列字在所有平台預設開啟；其他 Plus 功能在 Linux 預設關閉：fcitx5 在「設定 → 新酷音 → 設定」開啟 `Plus:` 開頭的選項；IBus 用「新酷音 Plus」設定程式開啟。

## 安裝：macOS

需要 macOS 13 以上、Apple Silicon。套件**沒有經過 Apple 簽章與公證**，Gatekeeper 會擋下來。

1. 對 `.pkg` 按右鍵 → 打開；若仍被擋，到「系統設定 → 隱私權與安全性」按「強制打開」。
2. 安裝程式會自動註冊並啟用輸入法。輸入法選單（選單列右上角）應該會出現「新酷音 Plus」。
3. 如果沒有出現：**登出再登入**，然後到「系統設定 → 鍵盤 → 文字輸入 → 輸入方式 → 編輯… → ＋」，選左側「繁體中文」，右側找到「新酷音 Plus」加入。
4. 還是找不到時，在終端機執行：

   ```bash
   "/Library/Input Methods/ChewingPlus.app/Contents/MacOS/ChewingPlus" --register
   ```

   輸出 `enable im.chewing.inputmethod.ChewingPlus.Bopomofo: ok` 代表已註冊。

Plus 功能在 macOS 預設開啟，可從選單列的輸入法選單個別關閉。

**語音輸入預設不啟用。** 安裝程式的「自訂」畫面有「啟用語音」選項，勾選後安裝完就會啟動語音服務；也可以之後從輸入法選單的「啟用語音」開啟或關閉。模型載入後語音服務約佔 260 MB 記憶體。從舊版升級時，已啟用的語音維持啟用。預設語音快速鍵是按住右 Option 說話，可在「快速鍵…」修改。

語音輸入需要 Homebrew 的 OpenCC（`brew install opencc`）做繁體轉換，沒有安裝時會輸出簡體；第一次錄音時要允許麥克風權限。

## 已知限制

- macOS 版已在實機測試；Linux 版（fcitx5 改寫、IBus 中／英切換、IBus 設定視窗）已通過自動測試，但尚未在真實桌面上完整驗證。
- 免打聲調時，最後一個音節要等下一個聲母或空白鍵才會轉換。
- 從舊版升級的 macOS 使用者，`/Library/Input Methods` 可能留有舊 ID 的副本，可執行 `macos/uninstall.sh` 清除後重裝。
- macOS 套件未簽章、未公證。
- Linux 套件只提供 Arch Linux x86_64。
