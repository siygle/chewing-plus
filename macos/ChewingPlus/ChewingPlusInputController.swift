import Carbon
import Cocoa
import InputMethodKit
import os.log

private let imLog = OSLog(subsystem: "im.chewing.inputmethod.ChewingPlus", category: "controller")

/// Native InputMethodKit controller. Drawing is IMKCandidates (AppKit).
/// Composition is libchewing-plus-core. Voice is a version-1 client of the
/// voxkit launchd helper. This process does not load a model.
/// The Objective-C name matches InputMethodServerControllerClass.
@objc(ChewingPlusInputController)
final class ChewingPlusInputController: IMKInputController {
    private var session: OpaquePointer?
    private var voice = UnsafeMutablePointer<voxkit_client>.allocate(capacity: 1)
    private enum VoiceState { case idle, recording, transcribing }
    private var voiceState = VoiceState.idle {
        didSet {
            transcribeTimeout?.cancel()
            if voiceState == .transcribing {
                // A reply that never comes must not leave 「辨識中…」 up.
                let work = DispatchWorkItem { [weak self] in
                    guard let self, self.voiceState == .transcribing else { return }
                    self.voiceState = .idle
                    HintWindow.shared.flash("語音辨識逾時", client: self.lastClient)
                }
                transcribeTimeout = work
                DispatchQueue.main.asyncAfter(deadline: .now() + 30, execute: work)
            }
        }
    }
    private var transcribeTimeout: DispatchWorkItem?
    private var voiceActive: Bool { voiceState != .idle }
    private var voiceReq: UInt64 = 0
    private var voiceSource: DispatchSourceRead?
    /// The shortcut that started the running round; only it ends the round.
    private var voiceSetting = ShortcutSetting.voice
    private var voiceTranslating = false
    private var voiceStartedAt = Date.distantPast
    /// Translate capability of the connected daemon: -1 not asked yet.
    private var voiceCaps: Int32 = -1
    /// Shorter presses are accidental taps (or the key used as a modifier
    /// in a chord) and are cancelled instead of transcribed.
    private static let voiceMinHold: TimeInterval = 0.25
    private var shortcutObserver: NSObjectProtocol?
    /// One candidate panel for the process. IMKServer keeps a raw pointer
    /// to the panel it last showed; a per-controller panel is freed with its
    /// controller and the server then messages a dead object (crashes in
    /// _serverGetsFirstShotAtWindowEvents and deactivateServer).
    private static var sharedPanel: IMKCandidates?
    private var panel: IMKCandidates? { Self.sharedPanel }
    private var lastClient: IMKTextInput?
    private var commitHook: chewing_plus_callbacks?
    private var sensitive = false
    /// Lone Left Shift switches 中/英 (fcitx5 does this on Linux). In English
    /// mode every key goes straight to the app. One mode for all apps, shown
    /// in the menu bar by ModeIndicator.
    private var englishMode: Bool {
        get { ModeIndicator.shared.english }
        set { ModeIndicator.shared.english = newValue }
    }
    private var shiftDownAt: TimeInterval = 0
    private var shiftAlone = false
    /// Double tap of a lone-modifier English shortcut.
    private var tapDownAt: TimeInterval = -1
    private var lastTapUpAt: TimeInterval = -1
    private var panelShown = false
    private var drawnRevision: UInt64 = 0

    override init!(server: IMKServer!, delegate: Any!, client: Any!) {
        super.init(server: server, delegate: delegate, client: client)
        voxkit_client_init(voice)
        if Self.sharedPanel == nil {
            let made = IMKCandidates(server: server,
                                     panelType: kIMKSingleColumnScrollingCandidatePanel)
            made?.setSelectionKeys(["1", "2", "3", "4", "5", "6", "7", "8", "9"])
            Self.sharedPanel = made
        }
        shortcutObserver = NotificationCenter.default.addObserver(
            forName: .chewingPlusShortcutsChanged, object: nil, queue: .main
        ) { [weak self] _ in
            guard let self, let session = self.session else { return }
            self.applyFeatures(session)
        }
    }

    deinit {
        if let shortcutObserver { NotificationCenter.default.removeObserver(shortcutObserver) }
        voiceSource?.cancel()
        voxkit_client_close(voice)
        voice.deallocate()
        if let session { SessionPool.shared.checkIn(session) }
    }

    override func menu() -> NSMenu! {
        let menu = NSMenu(title: "新酷音 Plus")
        for row in Settings.menuRows {
            var title = row.label
            if row.key == "voice_translate", let key = ShortcutSetting.translate.value {
                title += "（\(key.title)）"
            }
            let item = NSMenuItem(title: title, action: #selector(toggleFeature(_:)),
                                  keyEquivalent: "")
            item.target = self
            item.representedObject = row.key
            item.state = Settings.isOn(row.key) ? .on : .off
            menu.addItem(item)
        }
        let current = Int(Settings.value("initials_min") ?? "2") ?? 2
        let initialsMenu = NSMenu(title: "字首注音")
        for n in 2...6 {
            let choice = NSMenuItem(title: "\(n) 個以上", action: #selector(pickInitialsMin(_:)),
                                    keyEquivalent: "")
            choice.target = self
            choice.representedObject = n
            choice.state = n == current ? .on : .off
            initialsMenu.addItem(choice)
        }
        let initials = NSMenuItem(title: "字首注音幾個以上開始建議：\(current)",
                                  action: nil, keyEquivalent: "")
        initials.submenu = initialsMenu
        menu.addItem(initials)
        let target = Settings.value("voice_translate_to") ?? "en"
        let targetMenu = NSMenu(title: "語音翻譯成")
        for (code, name) in Self.translateTargets {
            let choice = NSMenuItem(title: name, action: #selector(pickTranslateTarget(_:)),
                                    keyEquivalent: "")
            choice.target = self
            choice.representedObject = code
            choice.state = code == target ? .on : .off
            targetMenu.addItem(choice)
        }
        let targetItem = NSMenuItem(
            title: "語音翻譯成：\(Self.translateTargets.first { $0.0 == target }?.1 ?? target)",
            action: nil, keyEquivalent: "")
        targetItem.submenu = targetMenu
        menu.addItem(targetItem)
        let voice = NSMenuItem(title: "啟用語音", action: #selector(toggleVoice(_:)),
                               keyEquivalent: "")
        voice.target = self
        voice.state = VoiceAgent.enabled ? .on : .off
        menu.addItem(voice)
        let indicator = NSMenuItem(title: "選單列顯示「中／英」",
                                   action: #selector(toggleIndicator(_:)), keyEquivalent: "")
        indicator.target = self
        indicator.state = Settings.modeIndicator ? .on : .off
        menu.addItem(indicator)
        menu.addItem(.separator())
        let keys = NSMenuItem(title: "快速鍵…", action: #selector(openShortcuts(_:)),
                              keyEquivalent: "")
        keys.target = self
        menu.addItem(keys)
        let item = NSMenuItem(title: "語音模型…", action: #selector(openModels(_:)),
                              keyEquivalent: "")
        item.target = self
        menu.addItem(item)
        return menu
    }

    @objc private func toggleFeature(_ sender: Any?) {
        // IMK hands menu actions a dictionary whose kIMKCommandMenuItemName
        // is the NSMenuItem.
        let item = (sender as? [String: Any])?[kIMKCommandMenuItemName] as? NSMenuItem
            ?? sender as? NSMenuItem
        guard let key = item?.representedObject as? String else { return }
        Settings.set(key, Settings.isOn(key) ? "0" : "1")
        if let session { applyFeatures(session) }
        if key == "shift_toggle" && !Settings.isOn(key) { englishMode = false }
    }

    private static let translateTargets: [(String, String)] = [
        ("en", "英文"), ("ja", "日文"), ("ko", "韓文"), ("zh-Hant", "繁體中文"),
        ("zh-Hans", "簡體中文"), ("fr", "法文"), ("de", "德文"), ("es", "西班牙文"),
        ("vi", "越南文"), ("th", "泰文"),
    ]

    @objc private func pickTranslateTarget(_ sender: Any?) {
        let item = (sender as? [String: Any])?[kIMKCommandMenuItemName] as? NSMenuItem
            ?? sender as? NSMenuItem
        guard let code = item?.representedObject as? String else { return }
        Settings.set("voice_translate_to", code)
        if let session { applyFeatures(session) }
    }

    @objc private func pickInitialsMin(_ sender: Any?) {
        let item = (sender as? [String: Any])?[kIMKCommandMenuItemName] as? NSMenuItem
            ?? sender as? NSMenuItem
        guard let n = item?.representedObject as? Int else { return }
        Settings.set("initials_min", String(n))
        if let session { applyFeatures(session) }
    }

    @objc private func toggleVoice(_: Any?) {
        if voiceActive { _ = voiceCancel() }
        closeVoice()
        let on = !VoiceAgent.enabled
        if !VoiceAgent.set(on) {
            HintWindow.shared.flash(on ? "無法啟用語音服務" : "無法停用語音服務",
                                    client: lastClient)
        } else if on {
            HintWindow.shared.flash("語音已啟用：按住語音快速鍵說話", client: lastClient)
        }
    }

    @objc private func toggleIndicator(_: Any?) {
        Settings.modeIndicator.toggle()
        ModeIndicator.shared.setVisible(true)
    }

    @objc private func openShortcuts(_: Any?) {
        if voiceActive { _ = voiceCancel() }
        ShortcutPreferences.shared.show()
    }

    @objc private func openModels(_: Any?) {
        ModelManagerWindow.shared.show()
    }

    override func recognizedEvents(_ sender: Any!) -> Int {
        let mask = NSEvent.EventTypeMask.keyDown.union(.keyUp).union(.flagsChanged)
        return Int(mask.rawValue)
    }

    override func handle(_ event: NSEvent!, client sender: Any!) -> Bool {
        if session == nil { attachSession() }
        guard let event, let session else { return false }
        lastClient = sender as? IMKTextInput
        if secure() {
            if !sensitive {
                sensitive = true
                chewing_plus_set_sensitive(session, 1)
                if voiceActive { _ = voiceCancel() }
                chewing_plus_reset(session)
                publish(session)
            }
            return false
        }
        if sensitive {
            sensitive = false
            chewing_plus_set_sensitive(session, 0)
        }
        if let consumed = voiceKey(event) { return consumed }
        if voiceActive && event.type == .keyDown {
            _ = voiceCancel()
        }
        if englishShortcut(event) {
            if chewing_plus_toggle_english(session) != 0 { publish(session) }
            return true
        }
        // Command shortcuts belong to the app.
        if event.modifierFlags.contains(.command) { return false }
        if lonelyShift(event) {
            toggleEnglishMode(session)
            return true
        }
        if englishMode { return false }
        if pickSuggestion(event, session: session) { return true }
        if selectDigit(event, session: session) { return true }
        var key = chewing_plus_key_event(
            key: 0, ascii: 0, mods: 0, keycode: Self.x11Keycode(event.keyCode), time_ms: -1)
        if event.modifierFlags.contains(.shift) { key.mods |= UInt32(CHEWING_PLUS_MOD_SHIFT) }
        if event.modifierFlags.contains(.control) { key.mods |= UInt32(CHEWING_PLUS_MOD_CTRL) }
        if event.modifierFlags.contains(.option) { key.mods |= UInt32(CHEWING_PLUS_MOD_ALT) }
        let release = event.type == .keyUp || (event.type == .flagsChanged
            && !event.modifierFlags.contains(.shift))
        if release { key.mods |= UInt32(CHEWING_PLUS_MOD_RELEASE) }
        if !Self.fill(&key, from: event) { return false }
        let consumed = chewing_plus_key(session, &key) != 0
        // Only talk to the client when something visible changed: every
        // setMarkedText is a round trip to the app.
        if Self.revision(session) != drawnRevision { publish(session) }
        // A key the core does not want (Backspace on an empty buffer, a key
        // release, a lone Shift) goes back to the app.
        return consumed
    }

    override func candidateSelected(_ candidateString: NSAttributedString!) {
        guard let session, let raw = candidateString?.string else { return }
        let count = chewing_plus_candidate_count(session)
        var index: Int32 = 0
        while index < count {
            if let text = chewing_plus_candidate(session, index), String(cString: text) == raw {
                chewing_plus_select(session, index)
                publish(session)
                return
            }
            index += 1
        }
    }

    override func candidates(_ sender: Any!) -> [Any]! {
        guard let session else { return [] }
        var list: [String] = []
        let count = chewing_plus_candidate_count(session)
        var index: Int32 = 0
        while index < count {
            if let text = chewing_plus_candidate(session, index) {
                list.append(String(cString: text))
            }
            index += 1
        }
        return list
    }

    /// Take a session from the pool for this client. IMK creates a
    /// controller per text field and keeps it around, so sessions are lent
    /// only while the field is active (activateServer..deactivateServer):
    /// memory follows the number of fields in use, not ever opened.
    private func attachSession() {
        guard session == nil, let taken = SessionPool.shared.checkOut() else { return }
        session = taken
        sensitive = false   // checkIn cleared the core's flag
        applyFeatures(taken)
        let userPtr = Unmanaged.passUnretained(self).toOpaque()
        var callbacks = chewing_plus_callbacks(commit: chewCommit, updated: nil, user: userPtr)
        commitHook = callbacks
        chewing_plus_set_callbacks(taken, &callbacks)
        chewing_plus_set_preceding_text(taken, chewPrecedingText, userPtr)
    }

    /// Commit what is being typed and hand the session back.
    private func detachSession() {
        guard let session else { return }
        var cursor = 0
        var hlStart = 0
        var hlLen = 0
        if let raw = chewing_plus_preedit(session, &cursor, &hlStart, &hlLen),
           !String(cString: raw).isEmpty {
            var enter = chewing_plus_key_event(
                key: UInt32(CHEWING_PLUS_KEY_RETURN), ascii: 0, mods: 0,
                keycode: 36, time_ms: -1)
            _ = chewing_plus_key(session, &enter)
        }
        lastClient?.setMarkedText("", selectionRange: NSRange(location: 0, length: 0),
                                  replacementRange: NSRange(location: NSNotFound, length: NSNotFound))
        if panelShown {
            panel?.hide()
            panelShown = false
        }
        var empty = chewing_plus_callbacks(commit: nil, updated: nil, user: nil)
        chewing_plus_set_callbacks(session, &empty)
        chewing_plus_set_preceding_text(session, nil, nil)   // user pointer dies with us
        self.session = nil
        SessionPool.shared.checkIn(session)
    }

    /// Plus toggles from the input menu (UserDefaults; Plus features on,
    /// easy symbol off by default).
    /// CHEWING_PLUS_FEATURES still overrides them for testing.
    /// Settings from the core's table, then the macOS-only pieces: phrase
    /// path, and English shortcuts detected here (configurable keys).
    private func applyFeatures(_ session: OpaquePointer) {
        if let phrases = Bundle.main.resourceURL?
            .appendingPathComponent("plus-phrases.bin").path {
            chewing_plus_set_phrases_path(session, phrases)
        }
        if let chars = Bundle.main.resourceURL?
            .appendingPathComponent("plus-chars.bin").path {
            chewing_plus_set_chars_path(session, chars)
        }
        Settings.apply(to: session)
        chewing_plus_set_english(session, Settings.isOn("english") ? 1 : 0,
                                 Settings.isOn("english") ? 1 : 0,
                                 ShortcutSetting.english.hintLabel)
        chewing_plus_set_english_combo(session, 0)
        chewing_plus_set_english_double_tap(session, 0, 300)
        // Lone Left Shift is handled here (menu-bar indicator, one mode for
        // all apps), not by the core's ASCII mode.
        chewing_plus_set_shift_toggle(session, 0)
    }

    fileprivate func insertCommitted(_ text: String) {
        lastClient?.insertText(text, replacementRange: NSRange(location: NSNotFound, length: NSNotFound))
    }

    /// The few characters before the insertion point, so the core can tell
    /// whether English quick insert follows Chinese. Marked text (the
    /// composition, which the core is about to replace) is not "before".
    fileprivate func textBeforeCursor() -> String {
        guard let client = lastClient else { return "" }
        let marked = client.markedRange()
        var location = client.selectedRange().location
        if marked.location != NSNotFound && marked.length > 0 {
            location = marked.location
        }
        guard location != NSNotFound, location > 0 else { return "" }
        let from = max(0, location - 4)   // up to two surrogate pairs
        let range = NSRange(location: from, length: location - from)
        return client.attributedSubstring(from: range)?.string ?? ""
    }

    /// Draw the core's view: marked text, hint line, suggestions, candidates.
    private func publish(_ session: OpaquePointer) {
        var view = chewing_plus_view()
        guard chewing_plus_get_view(session, &view) == 1 else { return }
        drawnRevision = view.revision
        let text = String(cString: view.preedit)
        // The core reports byte offsets; NSRange counts UTF-16 units.
        let utf8 = Array(text.utf8)
        func utf16(_ byte: Int) -> Int {
            let clamped = min(max(byte, 0), utf8.count)
            return String(decoding: utf8[0..<clamped], as: UTF8.self).utf16.count
        }
        let selection = view.highlight_length > 0
            ? NSRange(location: utf16(view.highlight_start),
                      length: utf16(view.highlight_start + view.highlight_length)
                          - utf16(view.highlight_start))
            : NSRange(location: utf16(view.cursor), length: 0)
        let replace = NSRange(location: NSNotFound, length: NSNotFound)
        lastClient?.setMarkedText(text, selectionRange: selection, replacementRange: replace)
        if voiceState == .idle {
            HintWindow.shared.show(String(cString: view.aux), client: lastClient)
        }
        HintWindow.shared.showSuggestions(Self.suggestions(session), client: lastClient)
        if view.candidate_count > 0 {
            panel?.update()
            panel?.show(kIMKLocateCandidatesBelowHint)
            panelShown = true
        } else if panelShown {
            panel?.hide()
            panelShown = false
        }
    }

    private static func suggestions(_ session: OpaquePointer) -> [String] {
        (0..<Int(chewing_plus_suggestion_count(session))).compactMap { i in
            chewing_plus_suggestion(session, Int32(i)).map { String(cString: $0) }
        }
    }

    /// ⇧1…⇧9 picks a phrase suggestion while the list is showing. Plus panels
    /// and the candidate window keep their own digit keys.
    private func pickSuggestion(_ event: NSEvent, session: OpaquePointer) -> Bool {
        guard event.type == .keyDown,
              event.modifierFlags.intersection([.shift, .control, .option, .command]) == .shift,
              chewing_plus_suggestion_count(session) > 0,
              chewing_plus_candidate_count(session) == 0,
              chewing_plus_panel(session) == Int32(CHEWING_PLUS_PANEL_NONE),
              let digit = Self.digitRow[event.keyCode],
              digit <= Int(chewing_plus_suggestion_count(session)) else { return false }
        chewing_plus_select_suggestion(session, Int32(digit - 1))
        publish(session)
        return true
    }

    /// Mac keycodes of the digit row (by position, any layout).
    private static let digitRow: [UInt16: Int] = [18: 1, 19: 2, 20: 3, 21: 4, 23: 5,
                                                  22: 6, 26: 7, 28: 8, 25: 9]

    private static func revision(_ session: OpaquePointer) -> UInt64 {
        var view = chewing_plus_view()
        return chewing_plus_get_view(session, &view) == 1 ? view.revision : 0
    }

    /// Push-to-talk. Returns nil when the event is not a voice shortcut.
    /// The translate shortcut works the same way while 「語音翻譯」 is on.
    private func voiceKey(_ event: NSEvent) -> Bool? {
        if let consumed = talkKey(event, .voice) { return consumed }
        if translateKeyEnabled, let consumed = talkKey(event, .translate) { return consumed }
        return nil
    }

    private var translateKeyEnabled: Bool {
        guard Settings.isOn("voice_translate"), let key = ShortcutSetting.translate.value else {
            return false
        }
        return key != ShortcutSetting.voice.value
    }

    /// A lone modifier talks while held; a combo starts on its key press and
    /// stops when the key or any of its modifiers is released.
    private func talkKey(_ event: NSEvent, _ setting: ShortcutSetting) -> Bool? {
        guard let talk = setting.value else { return nil }
        // A round started by the other shortcut: this one is a plain key.
        if voiceActive && voiceSetting != setting { return nil }
        if let down = talk.modifierDown(event) {
            return down ? voiceStart(setting) : voiceStop()
        }
        if talk.isModifierOnly { return nil }
        if talk.matchesCombo(event) {
            if event.isARepeat || voiceActive { return true }
            return voiceStart(setting)
        }
        // Only the first release of a combo stops; the other keys of the
        // combo come up later and must not send a second stop.
        guard voiceState == .recording else {
            return event.type == .keyUp && event.keyCode == talk.keyCode && voiceActive
                ? true : nil
        }
        let keyReleased = event.type == .keyUp && event.keyCode == talk.keyCode
        let modifierReleased = event.type == .flagsChanged
            && !event.modifierFlags.contains(talk.flags)
        if keyReleased || modifierReleased {
            _ = voiceStop()
            return keyReleased
        }
        return nil
    }

    /// English mode / fill the hinted word. A lone modifier fires on its
    /// second tap (press and release, no key in between, each within 0.3 s);
    /// a combo fires on its key press.
    private func englishShortcut(_ event: NSEvent) -> Bool {
        guard Settings.isOn("english"), let key = ShortcutSetting.english.value,
              key != ShortcutSetting.voice.value,
              !(translateKeyEnabled && key == ShortcutSetting.translate.value) else { return false }
        if !key.isModifierOnly { return key.matchesCombo(event) && !event.isARepeat }
        guard let down = key.modifierDown(event) else {
            if event.type == .keyDown || event.type == .flagsChanged {
                tapDownAt = -1
                lastTapUpAt = -1
            }
            return false
        }
        let window = 0.3
        if down {
            tapDownAt = event.timestamp
            return false
        }
        defer { tapDownAt = -1 }
        guard tapDownAt >= 0, event.timestamp - tapDownAt <= window else {
            lastTapUpAt = -1
            return false
        }
        if lastTapUpAt >= 0, tapDownAt - lastTapUpAt <= window {
            lastTapUpAt = -1
            return true
        }
        lastTapUpAt = event.timestamp
        return false
    }

    /// True on the release of a Left Shift pressed and released by itself.
    private func lonelyShift(_ event: NSEvent) -> Bool {
        guard Settings.isOn("shift_toggle") else { return false }
        if event.type != .flagsChanged {
            if event.type == .keyDown { shiftAlone = false }
            return false
        }
        guard event.keyCode == 56 else {
            shiftAlone = false
            return false
        }
        let others: NSEvent.ModifierFlags = [.command, .control, .option]
        if event.modifierFlags.contains(.shift) {
            shiftAlone = event.modifierFlags.intersection(others).isEmpty
            shiftDownAt = event.timestamp
            return false
        }
        let alone = shiftAlone && event.timestamp - shiftDownAt < 0.5
        shiftAlone = false
        return alone
    }

    private func toggleEnglishMode(_ session: OpaquePointer) {
        if !englishMode {
            // Keep what was typed: commit the buffer before passing keys through.
            var cursor = 0
            var hlStart = 0
            var hlLen = 0
            if let raw = chewing_plus_preedit(session, &cursor, &hlStart, &hlLen),
               !String(cString: raw).isEmpty {
                var enter = chewing_plus_key_event(
                    key: UInt32(CHEWING_PLUS_KEY_RETURN), ascii: 0, mods: 0,
                    keycode: 36, time_ms: -1)
                _ = chewing_plus_key(session, &enter)
            }
            chewing_plus_reset(session)
            publish(session)
        }
        englishMode.toggle()
    }

    private func selectDigit(_ event: NSEvent, session: OpaquePointer) -> Bool {
        // Plus panels (English, emoji, calculator) select with Ctrl/Shift+N
        // inside the core; plain digits are text there.
        guard chewing_plus_panel(session) == Int32(CHEWING_PLUS_PANEL_NONE) else {
            return false
        }
        guard event.type == .keyDown,
              !event.modifierFlags.contains(.control),
              !event.modifierFlags.contains(.option),
              !event.modifierFlags.contains(.shift),
              let chars = event.characters, chars.count == 1,
              let digit = Int(chars), digit >= 1, digit <= 9 else {
            return false
        }
        if chewing_plus_candidate_count(session) < digit { return false }
        chewing_plus_select(session, Int32(digit - 1))
        publish(session)
        return true
    }

    private func secure() -> Bool {
        IsSecureEventInputEnabled()
    }

    private func voiceStart(_ setting: ShortcutSetting = .voice) -> Bool {
        guard session != nil, !secure() else { return true }
        let translate = setting == .translate
        if voxkit_client_fd(voice) < 0 {
            if voxkit_client_connect(voice, nil) != 0 {
                HintWindow.shared.flash(VoiceAgent.enabled
                    ? "語音服務啟動中，請稍候再試"
                    : "語音尚未啟用：輸入法選單 →「啟用語音」", client: lastClient)
                return true
            }
            watchVoice()
        }
        if translate {
            // Once per connection; an older daemon has no capability and
            // must never get mode=translate.
            if voiceCaps < 0 { voiceCaps = voxkit_client_supports_translate(voice, 400) }
            if voiceCaps == 0 {
                HintWindow.shared.flash("語音服務版本太舊，不支援翻譯", client: lastClient)
                return true
            }
        }
        voiceReq += 1
        let target = Settings.value("voice_translate_to").flatMap { $0.isEmpty ? nil : $0 } ?? "en"
        let mode: String? = translate ? "translate" : nil
        let targetArg: String? = translate ? target : nil
        var line = [CChar](repeating: 0, count: 512)
        let wrote = line.withUnsafeMutableBufferPointer { buffer -> Int32 in
            voxkit_proto_write_start_mode(buffer.baseAddress, buffer.count, voiceReq,
                                          "zh", "s2twp", nil, nil, 0, mode, targetArg)
        }
        if wrote > 0 {
            let sent = line.withUnsafeBufferPointer { buffer in
                voxkit_client_send(voice, buffer.baseAddress)
            }
            if sent == 0 {
                voiceSetting = setting
                voiceTranslating = translate
                voiceStartedAt = Date()
                voiceState = .recording
                HintWindow.shared.show(translate ? "🌐 翻譯聆聽中…" : "🎙 聆聽中…",
                                       client: lastClient)
            } else {
                closeVoice()
            }
        }
        return true
    }

    /// Key up: ask for the transcript. It arrives later on the socket and
    /// readVoice() inserts it, as the IBus engine does with a GLib fd watch.
    private func voiceStop() -> Bool {
        guard voiceState == .recording else { return voiceActive }
        if Date().timeIntervalSince(voiceStartedAt) < Self.voiceMinHold {
            return voiceCancel()
        }
        voiceState = .transcribing
        sendSimple("stop")
        HintWindow.shared.show(voiceTranslating ? "辨識並翻譯中…" : "辨識中…", client: lastClient)
        return true
    }

    private func voiceCancel() -> Bool {
        sendSimple("cancel")
        voiceState = .idle
        HintWindow.shared.show("", client: nil)
        return true
    }

    private func watchVoice() {
        let fd = voxkit_client_fd(voice)
        guard fd >= 0 else { return }
        let source = DispatchSource.makeReadSource(fileDescriptor: fd, queue: .main)
        source.setEventHandler { [weak self] in self?.readVoice() }
        source.resume()
        voiceSource = source
    }

    private func closeVoice() {
        voiceSource?.cancel()
        voiceSource = nil
        voxkit_client_close(voice)
        voiceState = .idle
        voiceCaps = -1  // the next daemon may be a different version
    }

    private func readVoice() {
        let msg = UnsafeMutablePointer<voxkit_message>.allocate(capacity: 1)
        defer { msg.deallocate() }
        while true {
            chewing_plus_mac_zero_message(msg)
            let rc = voxkit_client_next(voice, msg, 0, nil, 0)
            if rc == 0 { return }
            if rc < 0 {
                closeVoice()
                HintWindow.shared.show("語音服務已斷線", client: lastClient)
                return
            }
            let m = msg.pointee
            if m.req != 0 && m.req != voiceReq { continue }
            switch m.type {
            case VOXKIT_MSG_FINAL:
                voiceState = .idle
                HintWindow.shared.show("", client: nil)
                if let session {
                    // A translation goes through the same insert path; the
                    // daemon never applies the user dictionary to it.
                    chewing_plus_insert_text(session, chewing_plus_mac_message_text(msg))
                    publish(session)
                }
                if chewing_plus_mac_translate_mode(msg) != 0 && chewing_plus_mac_translated(msg) == 0 {
                    HintWindow.shared.flash(Self.translateFallbackText(
                        String(cString: chewing_plus_mac_translate_error(msg))), client: lastClient)
                }
            case VOXKIT_MSG_PARTIAL:
                HintWindow.shared.show(String(cString: chewing_plus_mac_message_tentative(msg)),
                                       client: lastClient)
            case VOXKIT_MSG_DISCARDED:
                voiceState = .idle
                HintWindow.shared.flash("沒有聽到聲音", client: lastClient)
            case VOXKIT_MSG_ERROR:
                // A stray stop (not-recording) is harmless; the real session
                // is still on its way.
                let code = withUnsafeBytes(of: m.code) { raw in
                    String(cString: raw.bindMemory(to: CChar.self).baseAddress!)
                }
                if code == "not-recording" { continue }
                voiceState = .idle
                HintWindow.shared.flash(Self.voiceErrorText(code,
                    String(cString: chewing_plus_mac_message_error(msg))), client: lastClient)
            default:
                break
            }
        }
    }

    private static func translateFallbackText(_ reason: String) -> String {
        switch reason {
        case "model-downloading": return "翻譯模型下載中，已輸入原文"
        case "license-required": return "請先在「語音模型…」同意翻譯模型授權，已輸入原文"
        case "plugin-missing": return "未安裝翻譯外掛，已輸入原文"
        case "bad-target": return "不支援的翻譯語言，已輸入原文"
        default: return "翻譯失敗，已輸入原文"
        }
    }

    private static func voiceErrorText(_ code: String, _ message: String) -> String {
        switch code {
        case "no-input": return "找不到麥克風，或沒有麥克風權限"
        case "model-not-ready", "bad-model": return "語音模型尚未就緒（輸入法選單 → 語音模型…）"
        default: return "語音錯誤：\(message.isEmpty ? code : message)"
        }
    }

    override func activateServer(_ sender: Any!) {
        super.activateServer(sender)
        lastClient = sender as? IMKTextInput ?? lastClient
        attachSession()
        ModeIndicator.shared.setVisible(true)
    }

    /// Switching apps or input methods: drop a recording and the hint.
    /// The 中/英 item hides once no app is using this input method.
    override func deactivateServer(_ sender: Any!) {
        if voiceState == .recording { _ = voiceCancel() }
        HintWindow.shared.show("", client: nil)
        HintWindow.shared.showSuggestions([], client: nil)
        ModeIndicator.shared.setVisible(false)
        detachSession()
        super.deactivateServer(sender)
    }

    private func sendSimple(_ type: String) {
        let line = "{\"v\":1,\"type\":\"\(type)\",\"req\":\(voiceReq)}\n"
        line.withCString { voxkit_client_send(voice, $0) }
    }

    private static func fill(_ key: inout chewing_plus_key_event, from event: NSEvent) -> Bool {
        switch event.keyCode {
        case 36, 76: key.key = UInt32(CHEWING_PLUS_KEY_RETURN)
        case 51: key.key = UInt32(CHEWING_PLUS_KEY_BACKSPACE)
        case 53: key.key = UInt32(CHEWING_PLUS_KEY_ESCAPE)
        case 48: key.key = UInt32(CHEWING_PLUS_KEY_TAB)
        case 49:
            key.key = UInt32(CHEWING_PLUS_KEY_SPACE)
            key.ascii = 32
        case 123: key.key = UInt32(CHEWING_PLUS_KEY_LEFT)
        case 124: key.key = UInt32(CHEWING_PLUS_KEY_RIGHT)
        case 125: key.key = UInt32(CHEWING_PLUS_KEY_DOWN)
        case 126: key.key = UInt32(CHEWING_PLUS_KEY_UP)
        case 115: key.key = UInt32(CHEWING_PLUS_KEY_HOME)
        case 119: key.key = UInt32(CHEWING_PLUS_KEY_END)
        case 116: key.key = UInt32(CHEWING_PLUS_KEY_PAGE_UP)
        case 121: key.key = UInt32(CHEWING_PLUS_KEY_PAGE_DOWN)
        case 117: key.key = UInt32(CHEWING_PLUS_KEY_DELETE)
        case 60: key.key = UInt32(CHEWING_PLUS_KEY_SHIFT_R)
        case 56: key.key = UInt32(CHEWING_PLUS_KEY_SHIFT_L)
        default:
            // event.characters keeps Shift (Shift+/ is "?"); the version
            // that ignores modifiers would hand the core "/" (ㄥ).
            let plain = !event.modifierFlags.contains(.control)
                && !event.modifierFlags.contains(.option)
            guard event.type == .keyDown,
                  let chars = plain ? event.characters : event.charactersIgnoringModifiers,
                  let scalar = chars.unicodeScalars.first,
                  scalar.value > 0x20, scalar.value < 0x7F else {
                return false
            }
            key.ascii = scalar.value
        }
        return true
    }

    /// The core reads X11 keycodes for the digit row (10...18 = 1...9), like
    /// fcitx5 and IBus. macOS keycodes are different numbers, so map the
    /// digits and send 0 for everything else.
    private static func x11Keycode(_ macKeyCode: UInt16) -> Int32 {
        let digits: [UInt16: Int32] = [18: 10, 19: 11, 20: 12, 21: 13, 23: 14,
                                       22: 15, 26: 16, 28: 17, 25: 18, 29: 19]
        return digits[macKeyCode] ?? 0
    }

    private static func env(_ name: String) -> String? {
        guard let raw = getenv(name) else { return nil }
        let value = String(cString: raw)
        return value.isEmpty ? nil : value
    }
}

private func chewCommit(user: UnsafeMutableRawPointer?, utf8: UnsafePointer<CChar>?) {
    guard let user, let utf8 else { return }
    let controller = Unmanaged<ChewingPlusInputController>.fromOpaque(user).takeUnretainedValue()
    controller.insertCommitted(String(cString: utf8))
}

private func chewPrecedingText(user: UnsafeMutableRawPointer?,
                               buf: UnsafeMutablePointer<CChar>?, cap: Int) -> Int32 {
    guard let user, let buf, cap > 1 else { return 0 }
    let controller = Unmanaged<ChewingPlusInputController>.fromOpaque(user).takeUnretainedValue()
    var bytes = Array(controller.textBeforeCursor().utf8)
    if bytes.count > cap - 1 {
        bytes = Array(bytes.suffix(cap - 1))
        while let first = bytes.first, first & 0xC0 == 0x80 { bytes.removeFirst() }
    }
    for (i, b) in bytes.enumerated() { buf[i] = CChar(bitPattern: b) }
    buf[bytes.count] = 0
    return Int32(bytes.count)
}

private extension Optional where Wrapped == String {
    func withOptionalCString<R>(_ body: (UnsafePointer<CChar>?) -> R) -> R {
        switch self {
        case .some(let value):
            return value.withCString { body($0) }
        case .none:
            return body(nil)
        }
    }
}

/// Settings come from the core's table (chewing_plus_setting_info), so a
/// setting added there shows up in the input menu. Values live in this
/// input method's UserDefaults under the table's keys.
///
/// macOS turns the Plus features on by default (Linux leaves them off).
/// Keys from 5.1.13-plus.3 and earlier are read once and migrated.
enum Settings {
    struct Row {
        let key: String
        let label: String
        let isBool: Bool
    }

    /// Settings shown as input-menu toggles, in table order. Engine options
    /// (layout, selection keys...) stay at the core's defaults here.
    static let menuRows: [Row] = {
        let hidden: Set<String> = ["space_digits", "english_hint",
                                   "space_as_selection", "add_phrase_forward",
                                   "choice_backward", "auto_shift_cursor"]
        var rows: [Row] = []
        var info = chewing_plus_setting_info()
        var i: Int32 = 0
        while chewing_plus_setting_info_at(i, &info) == 1 {
            let key = String(cString: info.key)
            if info.type == Int32(CHEWING_PLUS_SETTING_BOOL) && !hidden.contains(key) {
                rows.append(Row(key: key, label: String(cString: info.label_zh), isBool: true))
            }
            i += 1
        }
        return rows
    }()

    /// macOS defaults that differ from the core's.
    private static let macDefaults: [String: String] = [
        "emoji": "1", "calculator": "1", "space_letters": "1", "space_digits": "1",
        "shortcode": "1", "english": "1", "english_hint_commit": "1",
        "suggestions": "1", "shift_toggle": "1", "fuzzy": "1", "live_chars": "1",
    ]

    private static let store = UserDefaults.standard
    private static let prefix = "setting."

    static func value(_ key: String) -> String? {
        migrate()
        if let env = getenv("CHEWING_PLUS_FEATURES") {
            return String(cString: env).split(separator: ",")
                .contains(Substring(key)) ? "1" : "0"
        }
        return store.string(forKey: prefix + key) ?? macDefaults[key]
    }

    static func isOn(_ key: String) -> Bool { value(key) == "1" }

    static func set(_ key: String, _ value: String) {
        store.set(value, forKey: prefix + key)
        if key == "space_letters" { store.set(value, forKey: prefix + "space_digits") }
    }

    /// Push every stored or default value into a session.
    static func apply(to session: OpaquePointer) {
        var info = chewing_plus_setting_info()
        var i: Int32 = 0
        while chewing_plus_setting_info_at(i, &info) == 1 {
            let key = String(cString: info.key)
            if let v = value(key) { chewing_plus_set_setting(session, key, v) }
            i += 1
        }
    }

    /// The menu-bar 中/英 item is macOS-only, not a core setting.
    static var modeIndicator: Bool {
        get { store.object(forKey: prefix + "mode_indicator") as? Bool ?? true }
        set { store.set(newValue, forKey: prefix + "mode_indicator") }
    }

    private static var migrated = false
    private static func migrate() {
        guard !migrated else { return }
        migrated = true
        let old: [(String, [String])] = [
            ("emoji", ["emoji"]), ("calculator", ["calculator"]),
            ("space", ["space_letters", "space_digits"]),
            ("shortcode", ["shortcode"]), ("english", ["english"]),
            ("englishHintCommit", ["english_hint_commit"]),
            ("suggestions", ["suggestions"]), ("shiftToggle", ["shift_toggle"]),
            ("easySymbol", ["easy_symbol"]),
        ]
        for (oldKey, newKeys) in old {
            guard let on = store.object(forKey: oldKey) as? Bool else { continue }
            for k in newKeys where store.string(forKey: prefix + k) == nil {
                store.set(on ? "1" : "0", forKey: prefix + k)
            }
            store.removeObject(forKey: oldKey)
        }
        if let on = store.object(forKey: "modeIndicator") as? Bool {
            store.set(on, forKey: prefix + "mode_indicator")
            store.removeObject(forKey: "modeIndicator")
        }
    }
}

/// Small label under the cursor for the core's aux text (the English hint
/// "Control+Shift+Space → test"). fcitx5 shows the same string as aux.
final class HintWindow {
    static let shared = HintWindow()
    private let window: NSPanel
    private let stack = NSStackView()
    private let label = NSTextField(labelWithString: "")
    private var rows: [NSTextField] = []
    private var status = ""
    private var suggestions: [String] = []
    private var flashID = 0

    private init() {
        window = NSPanel(contentRect: .zero, styleMask: [.borderless, .nonactivatingPanel],
                         backing: .buffered, defer: true)
        window.level = .popUpMenu
        window.hasShadow = true
        window.isOpaque = false
        window.backgroundColor = NSColor.windowBackgroundColor.withAlphaComponent(0.97)
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 2
        stack.edgeInsets = NSEdgeInsets(top: 4, left: 6, bottom: 4, right: 8)
        label.font = NSFont.systemFont(ofSize: NSFont.smallSystemFontSize)
        label.textColor = .secondaryLabelColor
        stack.addArrangedSubview(label)
        window.contentView = stack
    }

    /// Show a status line that hides itself after `seconds`.
    func flash(_ text: String, client: IMKTextInput?, seconds: Double = 3) {
        show(text, client: client)
        flashID += 1
        let id = flashID
        DispatchQueue.main.asyncAfter(deadline: .now() + seconds) { [weak self] in
            guard let self, self.flashID == id else { return }
            self.status = ""
            self.layout(client: client)
        }
    }

    /// Status line (English hint, voice state). "" clears it.
    func show(_ text: String, client: IMKTextInput?) {
        flashID += 1
        status = text
        layout(client: client)
    }

    /// Phrase suggestions, picked with ⇧1…⇧9. [] clears them.
    func showSuggestions(_ list: [String], client: IMKTextInput?) {
        suggestions = list
        layout(client: client)
    }

    private func layout(client: IMKTextInput?) {
        guard let client, !(status.isEmpty && suggestions.isEmpty) else {
            window.orderOut(nil)
            return
        }
        label.stringValue = status
        label.isHidden = status.isEmpty
        while rows.count < suggestions.count {
            let row = NSTextField(labelWithString: "")
            row.font = NSFont.systemFont(ofSize: NSFont.systemFontSize + 1)
            stack.addArrangedSubview(row)
            rows.append(row)
        }
        for (i, row) in rows.enumerated() {
            if i < suggestions.count {
                let text = NSMutableAttributedString(
                    string: "⇧\(i + 1) ",
                    attributes: [.foregroundColor: NSColor.secondaryLabelColor,
                                 .font: NSFont.monospacedDigitSystemFont(
                                    ofSize: NSFont.smallSystemFontSize, weight: .regular)])
                text.append(NSAttributedString(string: suggestions[i]))
                row.attributedStringValue = text
                row.isHidden = false
            } else {
                row.isHidden = true
            }
        }
        var rect = NSRect.zero
        _ = client.attributes(forCharacterIndex: 0, lineHeightRectangle: &rect)
        let size = stack.fittingSize
        window.setFrame(NSRect(x: rect.minX, y: rect.minY - size.height - 4,
                               width: size.width, height: size.height), display: true)
        window.orderFront(nil)
    }
}

/// 「中」/「英」 in the menu bar, like 超注音. Clicking it switches the mode.
/// Shown while this input method is active (IMK deactivates the old client
/// before activating the next, so the item does not flicker between apps).
final class ModeIndicator: NSObject {
    static let shared = ModeIndicator()
    private var item: NSStatusItem?
    private var hideWork: DispatchWorkItem?

    var english = false {
        didSet { refresh() }
    }

    func setVisible(_ visible: Bool) {
        hideWork?.cancel()
        if visible {
            guard Settings.modeIndicator else {
                remove()
                return
            }
            if item == nil {
                let created = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
                created.autosaveName = "im.chewing.inputmethod.ChewingPlus.mode"
                created.button?.target = self
                created.button?.action = #selector(clicked)
                item = created
            }
            refresh()
        } else {
            // Switching between apps deactivates then activates again;
            // only remove the item if nothing came back.
            let work = DispatchWorkItem { [weak self] in self?.remove() }
            hideWork = work
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.5, execute: work)
        }
    }

    private func remove() {
        if let item { NSStatusBar.system.removeStatusItem(item) }
        item = nil
    }

    private func refresh() {
        guard let button = item?.button else { return }
        button.title = english ? "英" : "中"
        button.font = NSFont.systemFont(ofSize: 15, weight: .semibold)
        button.toolTip = english ? "新酷音 Plus：英文（按左 Shift 切回中文）"
                                 : "新酷音 Plus：中文（按左 Shift 切換英文）"
    }

    @objc private func clicked() {
        english.toggle()
    }
}

/// libchewing sessions shared by all controllers. Each costs about 10 MB
/// (libchewing's own tables); the Plus dictionaries are already shared
/// (DataCache). A few idle sessions are kept for quick reuse.
final class SessionPool {
    static let shared = SessionPool()
    private var idle: [OpaquePointer] = []
    private let keepIdle = 2
    private var paths: (sys: String?, user: String?, emoji: String?, english: String?)?

    func checkOut() -> OpaquePointer? {
        if let reused = idle.popLast() {
            chewing_plus_reset(reused)
            return reused
        }
        let p = resolvedPaths()
        let made: OpaquePointer? = p.sys.withOptionalCString { sysPtr in
            p.user.withOptionalCString { userPtr in chewing_plus_new(sysPtr, userPtr) }
        }
        guard let made else { return nil }
        p.emoji.withOptionalCString { emojiPtr in
            p.english.withOptionalCString { englishPtr in
                chewing_plus_set_data_paths(made, emojiPtr, "", englishPtr, "")
            }
        }
        return made
    }

    func checkIn(_ session: OpaquePointer) {
        chewing_plus_reset(session)
        chewing_plus_set_sensitive(session, 0)
        if idle.count < keepIdle {
            idle.append(session)
        } else {
            chewing_plus_free(session)
        }
    }

    private func resolvedPaths() -> (sys: String?, user: String?, emoji: String?, english: String?) {
        if let paths { return paths }
        let res = Bundle.main.resourceURL
        let env = { (name: String) -> String? in
            guard let raw = getenv(name) else { return nil }
            let value = String(cString: raw)
            return value.isEmpty ? nil : value
        }
        let resolved = (
            sys: env("CHEWING_PATH") ?? res?.appendingPathComponent("libchewing", isDirectory: true).path,
            user: env("CHEWING_USER_PATH"),
            emoji: env("CHEWING_PLUS_EMOJI") ?? res?.appendingPathComponent("plus-emoji-shortcodes.txt").path,
            english: env("CHEWING_PLUS_ENGLISH") ?? res?.appendingPathComponent("plus-english.txt").path)
        paths = resolved
        return resolved
    }
}

/// The voxkit LaunchAgent written by the installer. It is installed with
/// Disabled=true unless the installer's 「啟用語音」 choice was ticked; the
/// input menu flips it here. The daemon keeps the speech model in memory
/// (about 260 MB once a model is downloaded), so it is off unless asked for.
enum VoiceAgent {
    static let label = "im.chewing.plus.voxkit"
    static var plistURL: URL {
        FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/LaunchAgents/\(label).plist")
    }

    static var enabled: Bool {
        guard let dict = NSDictionary(contentsOf: plistURL) else { return false }
        return !(dict["Disabled"] as? Bool ?? false)
    }

    /// Write Disabled and load or unload the agent. False if the agent file
    /// is missing (installed without the pkg) or launchctl failed.
    @discardableResult
    static func set(_ on: Bool) -> Bool {
        guard let dict = NSMutableDictionary(contentsOf: plistURL) else { return false }
        dict["Disabled"] = !on
        guard dict.write(to: plistURL, atomically: true) else { return false }
        let domain = "gui/\(getuid())"
        _ = launchctl(["bootout", "\(domain)/\(label)"])
        return on ? launchctl(["bootstrap", domain, plistURL.path]) : true
    }

    private static func launchctl(_ args: [String]) -> Bool {
        let p = Process()
        p.executableURL = URL(fileURLWithPath: "/bin/launchctl")
        p.arguments = args
        p.standardOutput = FileHandle.nullDevice
        p.standardError = FileHandle.nullDevice
        do { try p.run() } catch { return false }
        p.waitUntilExit()
        return p.terminationStatus == 0
    }
}
