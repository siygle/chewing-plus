import Carbon
import Cocoa

/// A user-recorded shortcut. Either a lone modifier key (Right Option,
/// Right Command, ...) or a combination: modifiers plus one key
/// (Control+Option+Space). Combos avoid clashing with system and app
/// shortcuts; a lone modifier is quicker to hold.
struct Shortcut: Codable, Equatable {
    /// Virtual key code of the key: a modifier's own code for a lone
    /// modifier, otherwise the non-modifier key.
    var keyCode: UInt16
    /// Device-independent modifier flags that must be held (rawValue of
    /// NSEvent.ModifierFlags). Empty for a lone modifier.
    var modifiers: UInt
    var isModifierOnly: Bool

    static let modifierKeyCodes: [UInt16: NSEvent.ModifierFlags] = [
        54: .command, 55: .command,     // right, left Command
        56: .shift, 60: .shift,         // left, right Shift
        58: .option, 61: .option,       // left, right Option
        59: .control, 62: .control,     // left, right Control
        63: .function,
    ]

    static let relevantFlags: NSEvent.ModifierFlags = [.command, .shift, .option, .control, .function]

    var flags: NSEvent.ModifierFlags { NSEvent.ModifierFlags(rawValue: modifiers) }

    var title: String {
        if isModifierOnly { return Self.keyName(keyCode) }
        var parts: [String] = []
        if flags.contains(.control) { parts.append("⌃") }
        if flags.contains(.option) { parts.append("⌥") }
        if flags.contains(.shift) { parts.append("⇧") }
        if flags.contains(.command) { parts.append("⌘") }
        if flags.contains(.function) { parts.append("fn") }
        return parts.joined() + Self.keyName(keyCode)
    }

    static func keyName(_ code: UInt16) -> String {
        let names: [UInt16: String] = [
            54: "右 ⌘", 55: "左 ⌘", 56: "左 ⇧", 60: "右 ⇧", 58: "左 ⌥", 61: "右 ⌥",
            59: "左 ⌃", 62: "右 ⌃", 63: "fn", 49: "Space", 36: "Return", 48: "Tab",
            51: "Delete", 53: "Esc", 123: "←", 124: "→", 125: "↓", 126: "↑",
            122: "F1", 120: "F2", 99: "F3", 118: "F4", 96: "F5", 97: "F6",
            98: "F7", 100: "F8", 101: "F9", 109: "F10", 103: "F11", 111: "F12",
        ]
        if let name = names[code] { return name }
        return character(for: code)?.uppercased() ?? "#\(code)"
    }

    /// The character the current keyboard layout puts on `code`.
    private static func character(for code: UInt16) -> String? {
        guard let source = TISCopyCurrentKeyboardLayoutInputSource()?.takeRetainedValue(),
              let ptr = TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData) else {
            return nil
        }
        let data = Unmanaged<CFData>.fromOpaque(ptr).takeUnretainedValue() as Data
        return data.withUnsafeBytes { raw -> String? in
            guard let layout = raw.bindMemory(to: UCKeyboardLayout.self).baseAddress else { return nil }
            var dead: UInt32 = 0
            var chars = [UniChar](repeating: 0, count: 4)
            var length = 0
            let status = UCKeyTranslate(layout, code, UInt16(kUCKeyActionDisplay), 0,
                                        UInt32(LMGetKbdType()), OptionBits(kUCKeyTranslateNoDeadKeysBit),
                                        &dead, chars.count, &length, &chars)
            return status == noErr && length > 0 ? String(utf16CodeUnits: chars, count: length) : nil
        }
    }

    /// A lone modifier is "down" on the flagsChanged that sets its flag.
    func modifierDown(_ event: NSEvent) -> Bool? {
        guard isModifierOnly, event.type == .flagsChanged, event.keyCode == keyCode,
              let flag = Self.modifierKeyCodes[keyCode] else { return nil }
        return event.modifierFlags.contains(flag)
    }

    /// keyDown of the combo with exactly these modifiers.
    func matchesCombo(_ event: NSEvent) -> Bool {
        guard !isModifierOnly, event.type == .keyDown, event.keyCode == keyCode else { return false }
        return event.modifierFlags.intersection(Self.relevantFlags) == flags
    }
}

/// Saved shortcuts. Voice: hold to talk. Translate: hold to talk, the
/// translation is typed (only while the 「語音翻譯」 setting is on). English:
/// tap twice (lone modifier) or press once (combo) to enter English mode /
/// fill the hint.
enum ShortcutSetting: String, CaseIterable {
    case voice, translate, english

    var title: String {
        switch self {
        case .voice: return "語音（按住說話）"
        case .translate: return "語音翻譯（按住說話）"
        case .english: return "英文模式／填入英文提示"
        }
    }

    var defaultValue: Shortcut? {
        switch self {
        case .voice: return Shortcut(keyCode: 61, modifiers: 0, isModifierOnly: true)    // 右 Option
        // Right Command: Mac keyboards have no right Control, and Right
        // Shift is the English double tap.
        case .translate: return Shortcut(keyCode: 54, modifiers: 0, isModifierOnly: true) // 右 ⌘
        case .english: return Shortcut(keyCode: 60, modifiers: 0, isModifierOnly: true)  // 右 Shift
        }
    }

    private var defaultsKey: String { "shortcut.\(rawValue)" }

    /// nil means the shortcut is turned off.
    var value: Shortcut? {
        get {
            let store = UserDefaults.standard
            if store.object(forKey: defaultsKey + ".off") as? Bool == true { return nil }
            guard let data = store.data(forKey: defaultsKey),
                  let shortcut = try? JSONDecoder().decode(Shortcut.self, from: data) else {
                return defaultValue
            }
            return shortcut
        }
        nonmutating set {
            let store = UserDefaults.standard
            if let newValue, let data = try? JSONEncoder().encode(newValue) {
                store.set(data, forKey: defaultsKey)
                store.removeObject(forKey: defaultsKey + ".off")
            } else {
                store.set(true, forKey: defaultsKey + ".off")
            }
            NotificationCenter.default.post(name: .chewingPlusShortcutsChanged, object: nil)
        }
    }

    func reset() {
        let store = UserDefaults.standard
        store.removeObject(forKey: defaultsKey)
        store.removeObject(forKey: defaultsKey + ".off")
        NotificationCenter.default.post(name: .chewingPlusShortcutsChanged, object: nil)
    }

    /// How the shortcut is used, shown under the recorder.
    var usage: String {
        guard let value else {
            return self == .translate
                ? "已關閉。即使尚未開啟「語音翻譯」，也可以先在這裡設定快速鍵。"
                : "已關閉"
        }
        switch self {
        case .voice:
            return "按住 \(value.title) 說話，放開出字"
        case .translate:
            let target = Settings.value("voice_translate_to") ?? "en"
            if Settings.isOn("voice_translate") {
                return "按住 \(value.title) 說話，放開輸入譯文（\(target)）"
            }
            return "快速鍵已設定為 \(value.title)。請再到輸入法選單勾選「語音翻譯」後才會生效。"
        case .english:
            return value.isModifierOnly ? "連按兩下 \(value.title)" : "按一下 \(value.title)"
        }
    }

    /// Text in the English hint: "右 ⇧ ×2 → GitHub".
    var hintLabel: String {
        guard let value else { return "Control+Shift+Space" }
        return value.isModifierOnly ? "\(value.title) ×2" : value.title
    }
}

extension Notification.Name {
    static let chewingPlusShortcutsChanged = Notification.Name("im.chewing.inputmethod.ChewingPlus.shortcuts")
}

/// Button that records the next key press. Press a lone modifier and let go
/// to record it, or hold modifiers and press a key for a combo. Esc cancels.
final class ShortcutRecorder: NSButton {
    var onRecord: ((Shortcut) -> Void)?
    private var recording = false
    private var monitor: Any?
    private var pendingModifier: UInt16?
    var shortcut: Shortcut? { didSet { refreshTitle() } }

    override init(frame: NSRect) {
        super.init(frame: frame)
        bezelStyle = .rounded
        target = self
        action = #selector(toggle)
        refreshTitle()
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    private func refreshTitle() {
        title = recording ? "請按下快速鍵…（Esc 取消）" : (shortcut?.title ?? "未設定")
    }

    @objc private func toggle() { recording ? stop() : start() }

    private func start() {
        recording = true
        pendingModifier = nil
        refreshTitle()
        monitor = NSEvent.addLocalMonitorForEvents(matching: [.keyDown, .flagsChanged]) { [weak self] event in
            self?.handle(event) ?? event
        }
    }

    private func stop() {
        recording = false
        if let monitor { NSEvent.removeMonitor(monitor) }
        monitor = nil
        refreshTitle()
    }

    private func handle(_ event: NSEvent) -> NSEvent? {
        let held = event.modifierFlags.intersection(Shortcut.relevantFlags)
        if event.type == .keyDown {
            if event.keyCode == 53 && held.isEmpty {   // Esc
                stop()
                return nil
            }
            // A combo needs at least one modifier, otherwise it would eat a
            // key that types text.
            guard !held.isEmpty else {
                NSSound.beep()
                return nil
            }
            finish(Shortcut(keyCode: event.keyCode, modifiers: held.rawValue, isModifierOnly: false))
            return nil
        }
        // flagsChanged: remember a lone modifier; record it when released
        // without another key in between.
        if let flag = Shortcut.modifierKeyCodes[event.keyCode] {
            if event.modifierFlags.contains(flag) {
                pendingModifier = held == flag ? event.keyCode : nil
            } else if let code = pendingModifier, code == event.keyCode, held.isEmpty {
                finish(Shortcut(keyCode: code, modifiers: 0, isModifierOnly: true))
            }
        }
        return nil
    }

    private func finish(_ value: Shortcut) {
        shortcut = value
        stop()
        onRecord?(value)
    }
}

/// 「快速鍵」window: one recorder per action, with off and reset.
final class ShortcutPreferences: NSObject {
    static let shared = ShortcutPreferences()
    private var window: NSWindow?
    private var recorders: [ShortcutSetting: ShortcutRecorder] = [:]
    private var notes: [ShortcutSetting: NSTextField] = [:]
    private let warning = NSTextField(labelWithString: "")

    func show() {
        if window == nil { build() }
        refresh()
        window?.center()
        window?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    private func build() {
        let win = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 580, height: 520),
                           styleMask: [.titled, .closable, .resizable], backing: .buffered, defer: false)
        win.title = "新酷音 Plus 快速鍵"
        win.isReleasedWhenClosed = false
        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 14
        stack.edgeInsets = NSEdgeInsets(top: 20, left: 20, bottom: 20, right: 20)
        stack.translatesAutoresizingMaskIntoConstraints = false

        let intro = NSTextField(wrappingLabelWithString:
            "點按鈕後按下快速鍵。可以是單一修飾鍵（例如右 ⌥），或組合鍵（例如 ⌃⌥Space）。組合鍵比較不會和系統或其他 App 的快速鍵衝突。語音、語音翻譯、英文三組快速鍵都會顯示在下面，即使還沒開啟「語音翻譯」。")
        intro.preferredMaxLayoutWidth = 520
        stack.addArrangedSubview(intro)

        // Always show all three, including translation, even when the
        // 「語音翻譯」 menu toggle is off. Do not rely on allCases order alone.
        let rows: [ShortcutSetting] = [.voice, .translate, .english]
        for setting in rows {
            let label = NSTextField(labelWithString: setting.title)
            label.font = .boldSystemFont(ofSize: NSFont.systemFontSize)
            label.setContentCompressionResistancePriority(.required, for: .horizontal)
            label.setContentCompressionResistancePriority(.required, for: .vertical)
            label.widthAnchor.constraint(greaterThanOrEqualToConstant: 200).isActive = true
            let recorder = ShortcutRecorder(frame: .zero)
            recorder.widthAnchor.constraint(equalToConstant: 200).isActive = true
            recorder.onRecord = { [weak self] value in
                setting.value = value
                self?.refresh()
            }
            let off = NSButton(title: "關閉", target: self, action: #selector(turnOff(_:)))
            off.identifier = NSUserInterfaceItemIdentifier(setting.rawValue)
            let reset = NSButton(title: "預設", target: self, action: #selector(resetOne(_:)))
            reset.identifier = NSUserInterfaceItemIdentifier(setting.rawValue)
            let row = NSStackView(views: [label, recorder, off, reset])
            row.spacing = 8
            row.setContentCompressionResistancePriority(.required, for: .vertical)
            let note = NSTextField(wrappingLabelWithString: "")
            note.textColor = .secondaryLabelColor
            note.font = .systemFont(ofSize: NSFont.smallSystemFontSize)
            note.preferredMaxLayoutWidth = 520
            note.setContentCompressionResistancePriority(.required, for: .vertical)
            stack.addArrangedSubview(row)
            stack.addArrangedSubview(note)
            recorders[setting] = recorder
            notes[setting] = note
        }
        warning.textColor = .systemRed
        warning.preferredMaxLayoutWidth = 520
        warning.setContentCompressionResistancePriority(.required, for: .vertical)
        stack.addArrangedSubview(warning)

        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        scroll.drawsBackground = false
        scroll.documentView = stack
        NSLayoutConstraint.activate([
            stack.topAnchor.constraint(equalTo: scroll.contentView.topAnchor),
            stack.leadingAnchor.constraint(equalTo: scroll.contentView.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: scroll.contentView.trailingAnchor),
            stack.widthAnchor.constraint(equalTo: scroll.contentView.widthAnchor),
        ])
        win.contentView = scroll
        window = win
    }

    @objc private func turnOff(_ sender: NSButton) {
        guard let raw = sender.identifier?.rawValue, let setting = ShortcutSetting(rawValue: raw) else { return }
        setting.value = nil
        refresh()
    }

    @objc private func resetOne(_ sender: NSButton) {
        guard let raw = sender.identifier?.rawValue, let setting = ShortcutSetting(rawValue: raw) else { return }
        setting.reset()
        refresh()
    }

    private func refresh() {
        for setting in [ShortcutSetting.voice, .translate, .english] {
            recorders[setting]?.shortcut = setting.value
            notes[setting]?.stringValue = setting.usage
        }
        let voice = ShortcutSetting.voice.value
        let english = ShortcutSetting.english.value
        let translate = ShortcutSetting.translate.value
        if voice != nil && voice == english {
            warning.stringValue = "語音和英文用了同一組快速鍵，英文會被停用。請換一組。"
        } else if translate != nil && translate == voice {
            warning.stringValue = "語音和語音翻譯用了同一組快速鍵，翻譯會被停用。請換一組。"
        } else if translate != nil && translate == english {
            warning.stringValue = "語音翻譯和英文用了同一組快速鍵，英文會被停用。請換一組。"
        } else {
            warning.stringValue = ""
        }
    }
}
