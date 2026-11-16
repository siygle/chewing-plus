新酷音 Plus：在新酷音上加入 emoji、計算機、英文模式與本機語音輸入。基於 [fcitx/fcitx5-chewing](https://github.com/fcitx/fcitx5-chewing) 與 libchewing 0.14.0-rc.1。功能說明請看 [README](https://github.com/siygle/fcitx5-chewing-plus#readme)。

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

Plus 功能預設關閉，在 fcitx5 設定 → 新酷音 → 設定中開啟 `Plus:` 開頭的選項。語音模型第一次使用前要下載（約 850 MB），可在設定或 `/usr/libexec/chewing-plus-models menu` 操作。

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

語音輸入需要 Homebrew 的 OpenCC（`brew install opencc`）做繁體轉換，第一次錄音時要允許麥克風權限。

## 已知限制

- macOS 版尚未在實機上完整驗證（錄音、候選窗位置、密碼欄位）。
- macOS 套件未簽章、未公證。
- Linux 套件只提供 Arch Linux x86_64。
