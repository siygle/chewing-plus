# fcitx5-chewing-plus

新酷音輸入法（fcitx5-chewing）的加強版，加上 emoji、計算機、英文模式與語音輸入等功能。
基於 [fcitx/fcitx5-chewing](https://github.com/fcitx/fcitx5-chewing) 與 libchewing 0.14。

所有新功能預設關閉，可在 **fcitx5 設定 → 新酷音 → 設定（齒輪）** 中，開啟 `Plus:` 開頭的選項。
在密碼等敏感欄位中，所有新功能都會自動停用。

## 功能

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

1. 按 `Ctrl+Shift+Space` 進入英文模式（組字區顯示 `[EN]`）
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
- 可以另外設定「連按兩下右 Shift」切換英文模式
- 快捷鍵可在設定中更改

### 英文提示
不切換模式，直接把英文字當注音打，例如打 `github`。
當輸入法判斷這串按鍵像英文單字時，會在下方提示：

```
Ctrl+Shift+Space → GitHub
```

按快捷鍵就會轉成英文；不理會的話，就照原本的注音繼續輸入。
提示只在確定是英文單字時才出現，不會干擾一般的注音輸入。

### 中英自動空格
中文與英文、數字相接時，會自動在中間加上空格：

```
你好Linux第5章  →  你好 Linux 第 5 章
```

英文字母與數字可以分別開關。

### 語音輸入
搭配 [fcitx5-voxkit](https://github.com/siygle/fcitx5-voxkit) 使用：

- **按住右 Alt** 說話，放開後輸出文字
- 辨識結果會插入目前游標的位置
- 會自動參考你自己加的詞，提高專有名詞的辨識率
- 全程在本機辨識，錄音不會上傳

詳細設定請見 fcitx5-voxkit 的說明。

## 自訂

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
- Emoji 短碼：[gemoji](https://github.com/github/gemoji)（MIT）
- 英文字典：[SCOWL](http://wordlist.aspell.net/)（MIT 類授權）

建置與開發說明請見 [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)。
