新酷音 Plus：在新酷音上加入 emoji、計算機、英文模式與本機語音輸入。基於 [fcitx/fcitx5-chewing](https://github.com/fcitx/fcitx5-chewing) 與 libchewing 0.14.0-rc.1。功能說明請看 [README](https://github.com/siygle/fcitx5-chewing-plus#readme)。

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
sudo pacman -U chewing-plus-fcitx5-*.pkg.tar.zst   # fcitx5
sudo pacman -U chewing-plus-ibus-*.pkg.tar.zst     # IBus：裝完執行 ibus restart，在 IBus 設定加入「新酷音 Plus」
```

從 5.1.13-plus.x 升級時，pacman 會詢問是否移除舊的 `libchewing-plus`、`fcitx5-chewing-plus`、`ibus-chewing-plus`、`fcitx5-voxkit`，回答 `y` 即可。兩個安裝檔互斥，切換框架時改裝另一個就好。

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
