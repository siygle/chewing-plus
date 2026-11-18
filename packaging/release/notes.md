新酷音 Plus：在新酷音上加入 emoji、計算機、英文模式與本機語音輸入。基於 [fcitx/fcitx5-chewing](https://github.com/fcitx/fcitx5-chewing) 與 libchewing 0.14.0-rc.1。功能說明請看 [README](https://github.com/siygle/fcitx5-chewing-plus#readme)。

## 這一版的變更（5.1.13-plus.4）

**fcitx5 版改用共用核心**
- fcitx5、IBus、macOS 三個平台現在用同一份核心邏輯，功能一致、修 bug 只需修一次。
- 原本 `chewing.conf` 的設定全部保留，升級不需重新設定。
- fcitx5 新增**詞語建議**（自動完成、字首注音，Shift+1～9 選取）與「英文提示時快速鍵直接填入」，在設定中開啟（預設關閉）。

**中／英切換**
- fcitx5：直接使用 fcitx5 內建的「暫時切換輸入法」鍵（預設單按左 Shift），會先送出正在打的字，fcitx5 指示器會顯示切換結果。
- IBus：新增單按左 Shift 切換中／英，IBus 面板顯示「中」／「英」，可點擊切換。在設定程式開啟（預設關閉）。

**設定統一**
- 所有設定改由同一份清單定義，IBus 設定程式與 macOS 選單由清單自動產生。
- 舊版的 IBus 與 macOS 設定會自動轉換，不需重新設定。

**效能**
- 詞語建議詞庫改為預先編譯：載入時間約 40 ms → 0.5 ms，記憶體約 15 MB → 4 MB。
- 英文字典記憶體約 17 MB → 7.6 MB。
- macOS 輸入法開啟所有 Plus 功能、正在輸入時約佔 30 MB（libchewing 約 12 MB、英文字典約 8 MB、詞語建議約 3 MB），多開輸入欄位共用同一份詞庫。

**上一版（5.1.13-plus.3）**：修正 macOS 無法安裝與打字（#7）、新增詞語建議（macOS、IBus）、選單列「中／英」、語音繁體輸出、各輸入欄位共用詞庫。

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

- 本版的 fcitx5 改寫、IBus 中／英切換、由設定清單產生的 IBus 設定視窗與 macOS 選單，已通過自動測試，但尚未在真實桌面上完整驗證。
- macOS 版的詞語建議清單、快速鍵錄製、選單列「中／英」尚未在多種 App 中完整驗證。
- 從舊版升級的 macOS 使用者，`/Library/Input Methods` 可能留有舊 ID 的副本，可執行 `macos/uninstall.sh` 清除後重裝。
- macOS 套件未簽章、未公證。
- Linux 套件只提供 Arch Linux x86_64。
