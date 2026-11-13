# fcitx5-chewing Plus

新酷音（fcitx5-chewing + libchewing 0.14）版的 Plus 功能，移植自
[fcitx5-mcbopomofo-plus](https://github.com/siygle/fcitx5-mcbopomofo/tree/plus)。
**只用 libchewing 公開的 C API**，不改 libchewing 本身（方案 A）。所有功能預設關閉。

在 fcitx5 設定 → 新酷音 → 齒輪，`Plus:` 開頭的選項。密碼欄位中全部停用。

| 功能 | 用法 |
|---|---|
| Emoji 候選 | 打「開心」後按 `↓`，候選裡有「開心😊」（標記 `☺`） |
| 計算器 | 組字區空白時按 `=`，打 `1+2*3`，Enter 上屏 `7` |
| 中英自動空格 | `你好Linux第5章` → `你好 Linux 第 5 章` |
| Emoji 短碼 | `;;smile`（快速連按兩下 `;`）。空白上屏首選、`Shift+1…9` 選其他 |
| 英文模式 | `Ctrl+Shift+Space`（或設定「連按兩下」的修飾鍵，例如右 Shift）進入 `[EN]`：空白上屏修正大小寫的字（`github`→`GitHub`），Enter 原樣，`Ctrl+1…9` 選候選，`Esc` 丟掉英文 |
| 英文提示 | 直接把英文當注音打，看起來像英文時下方提示 `… → GitHub`，按切換鍵才轉換 |

## 和小麥版的差異（新酷音引擎的限制）

- **英文會直接放進新酷音的組字區**（用新酷音本身的英文模式），所以前後的中文仍可以繼續編輯、選字。
- **Emoji 不是 ASCII，新酷音的組字區放不進去**：選 emoji 短碼或 emoji 候選時，會把「游標前的中文 + emoji + 游標後的中文」一起上屏。
- `` ` `` 是新酷音的符號表鍵，`;` `,` `.` `/` `-` 等是部分鍵盤配置的注音鍵，所以 emoji **單鍵**前綴不能用這些鍵；**連按兩下**（預設 `;;`）沒有這個限制。
- 中英切換：新酷音本來就有 Shift 切換中英，Plus 的英文模式是另一個獨立功能，兩者可以並用。

## 使用者檔案

`~/.local/share/fcitx5-chewing/`：
- `plus-emoji-shortcodes.txt`：`短碼 emoji [標籤…]`，蓋過內建
- `plus-english.txt`：一行一個英文字，優先且可改大小寫

內建資料：gemoji（MIT，`data/LICENSE.gemoji`）、SCOWL/ESDB（MIT 類，`data/LICENSE.scowl`）。

## 測試

需要 libchewing 0.14（`~/Work/libchewing` 建置後安裝到暫存目錄）。測試用獨立的 HOME 與資料夾，不會讀寫真正的使用者詞庫。

```bash
DESTDIR=/tmp/lc-stage cmake --install ~/Work/libchewing/build
export PKG_CONFIG_PATH=/tmp/lc-stage/usr/lib/pkgconfig PKG_CONFIG_SYSROOT_DIR=/tmp/lc-stage
cmake -B build -G Ninja && cmake --build build
test/run-plus-test.sh build /tmp/lc-stage/usr/share/libchewing /tmp/lc-stage/usr/lib
```
