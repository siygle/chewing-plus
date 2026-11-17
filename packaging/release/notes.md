新酷音 Plus：在新酷音上加入 emoji、計算機、英文模式與本機語音輸入。基於 [fcitx/fcitx5-chewing](https://github.com/fcitx/fcitx5-chewing) 與 libchewing 0.14.0-rc.1。功能說明請看 [README](https://github.com/siygle/fcitx5-chewing-plus#readme)。

## 這一版的變更

**macOS 終於可以正常使用**
- 修正：裝完 pkg 後系統找不到輸入法（#7），以及能選到卻完全不能打字。
- Backspace、Shift＋符號（例如 Shift+/ → `？`）、延遲都已修正。
- 單按左 Shift 切換中／英，選單列顯示「中」／「英」。
- 輸入法選單可開關所有 Plus 功能；「快速鍵…」可自訂語音與英文快速鍵（單一修飾鍵或組合鍵）。
- 語音輸出改為繁體，語音提示不再卡住。

**新功能：詞語建議**（macOS、IBus）
- 自動完成：打「中華」會建議 中華民國、中華隊…
- 字首注音：每字只打第一個注音，例如 ㄍㄒㄈㄘ → 恭喜發財。
- 用 Shift+1～9 選取，不影響原本的按鍵。詞庫來自 libchewing-data（約 12.9 萬詞）。
- fcitx5 版尚未支援。

**效能**
- 各輸入欄位共用詞庫，macOS 輸入法記憶體從約 200 MB 降到約 9 MB（閒置）。

## 下載

| 平台 | 檔案 | 說明 |
|---|---|---|
| Arch Linux（fcitx5） | `libchewing-plus-*.pkg.tar.zst`、`fcitx5-chewing-plus-*.pkg.tar.zst` | 必要 |
| Arch Linux（IBus） | `libchewing-plus-*.pkg.tar.zst`、`ibus-chewing-plus-*.pkg.tar.zst` | 必要 |
| Arch Linux 語音輸入 | `fcitx5-voxkit-*.pkg.tar.zst` | 選用，fcitx5 與 IBus 共用 |
| macOS（Apple Silicon） | `ChewingPlus-*-macos-arm64.pkg` | 內含輸入法與語音 daemon |

`SHA256SUMS` 是所有檔案的校驗碼。

## 安裝：Arch Linux

會取代系統的 `libchewing`（0.13）與 `fcitx5-chewing`。

```bash
# fcitx5
sudo pacman -U libchewing-plus-*.pkg.tar.zst fcitx5-chewing-plus-*.pkg.tar.zst
# 語音輸入（選用）
sudo pacman -U fcitx5-voxkit-*.pkg.tar.zst
systemctl --user enable --now voxkit.service
```

IBus 改裝 `ibus-chewing-plus-*.pkg.tar.zst`，然後 `ibus restart`，在 IBus 設定加入「新酷音 Plus」。

Plus 功能在 Linux 預設關閉：fcitx5 在「設定 → 新酷音 → 設定」開啟 `Plus:` 開頭的選項；IBus 用「新酷音 Plus」設定程式開啟。語音模型第一次使用前要下載（約 850 MB），可在設定或 `/usr/libexec/chewing-plus-models menu` 操作。

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

Plus 功能在 macOS 預設開啟，可從選單列的輸入法選單個別關閉。預設語音快速鍵是按住右 Option 說話，可在「快速鍵…」修改。

語音輸入需要 Homebrew 的 OpenCC（`brew install opencc`）做繁體轉換，沒有安裝時會輸出簡體；第一次錄音時要允許麥克風權限。

## 已知限制

- macOS 版的詞語建議清單、快速鍵錄製、選單列「中／英」尚未在多種 App 中完整驗證。
- fcitx5 版尚未支援詞語建議與「英文提示時快速鍵直接填入」。
- 從舊版升級的 macOS 使用者，`/Library/Input Methods` 可能留有舊 ID 的副本，可執行 `macos/uninstall.sh` 清除後重裝。
- macOS 套件未簽章、未公證。
- Linux 套件只提供 Arch Linux x86_64。
