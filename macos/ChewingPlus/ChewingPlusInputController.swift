import Carbon
import Cocoa
import InputMethodKit

/// Native InputMethodKit controller. Drawing is IMKCandidates (AppKit).
/// Composition is libchewing-plus-core. Voice is a version-1 client of the
/// voxkit launchd helper. This process does not load a model.
final class ChewingPlusInputController: IMKInputController {
    private var session: OpaquePointer?
    private var voice = UnsafeMutablePointer<voxkit_client>.allocate(capacity: 1)
    private var voiceActive = false
    private var voiceReq: UInt64 = 0
    private var panel: IMKCandidates?
    private var lastClient: IMKTextInput?
    private var commitHook: chewing_plus_callbacks?

    override init!(server: IMKServer!, delegate: Any!, client: Any!) {
        super.init(server: server, delegate: delegate, client: client)
        voxkit_client_init(voice)
        panel = IMKCandidates(server: server, panelType: kIMKSingleColumnScrollingCandidatePanel)
        panel?.setSelectionKeys(["1", "2", "3", "4", "5", "6", "7", "8", "9"])
        openSession()
    }

    deinit {
        voxkit_client_close(voice)
        voice.deallocate()
        chewing_plus_free(session)
    }

    override func menu() -> NSMenu! {
        let menu = NSMenu(title: "新酷音 Plus")
        let item = NSMenuItem(title: "语音模型…", action: #selector(openModels(_:)),
                              keyEquivalent: "")
        item.target = self
        menu.addItem(item)
        return menu
    }

    @objc private func openModels(_: Any?) {
        ModelPreferences.shared.show()
    }

    override func recognizedEvents(_ sender: Any!) -> Int {
        let mask = NSEvent.EventTypeMask.keyDown.union(.keyUp).union(.flagsChanged)
        return Int(mask.rawValue)
    }

    override func handle(_ event: NSEvent!, client sender: Any!) -> Bool {
        guard let event, let session else { return false }
        lastClient = sender as? IMKTextInput
        if secure() {
            chewing_plus_set_sensitive(session, 1)
            if voiceActive { _ = voiceCancel() }
            chewing_plus_reset(session)
            publish(session)
            return false
        }
        chewing_plus_set_sensitive(session, 0)
        if event.keyCode == 0x3D {
            let released = event.type == .keyUp
                || (event.type == .flagsChanged && !event.modifierFlags.contains(.option))
            return released ? voiceStop() : voiceStart()
        }
        if voiceActive && event.type == .keyDown {
            _ = voiceCancel()
        }
        if selectDigit(event, session: session) { return true }
        var key = chewing_plus_key_event(
            key: 0, ascii: 0, mods: 0, keycode: Int32(event.keyCode), time_ms: -1)
        if event.modifierFlags.contains(.shift) { key.mods |= UInt32(CHEWING_PLUS_MOD_SHIFT) }
        if event.modifierFlags.contains(.control) { key.mods |= UInt32(CHEWING_PLUS_MOD_CTRL) }
        if event.modifierFlags.contains(.option) { key.mods |= UInt32(CHEWING_PLUS_MOD_ALT) }
        if event.type == .keyUp { key.mods |= UInt32(CHEWING_PLUS_MOD_RELEASE) }
        if !Self.fill(&key, from: event) { return false }
        _ = chewing_plus_key(session, &key)
        publish(session)
        return true
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

    private func openSession() {
        let bundled = Bundle.main.resourceURL?
            .appendingPathComponent("libchewing", isDirectory: true).path
        let sys = Self.env("CHEWING_PATH") ?? bundled
        let user = Self.env("CHEWING_USER_PATH")
        session = sys.withOptionalCString { sysPtr in
            user.withOptionalCString { userPtr in
                chewing_plus_new(sysPtr, userPtr)
            }
        }
        guard let session else { return }
        let bundledEmoji = Bundle.main.resourceURL?
            .appendingPathComponent("plus-emoji-shortcodes.txt").path
        let bundledEnglish = Bundle.main.resourceURL?
            .appendingPathComponent("plus-english.txt").path
        let emoji = Self.env("CHEWING_PLUS_EMOJI") ?? bundledEmoji
        let english = Self.env("CHEWING_PLUS_ENGLISH") ?? bundledEnglish
        emoji.withOptionalCString { emojiPtr in
            english.withOptionalCString { englishPtr in
                chewing_plus_set_data_paths(session, emojiPtr, "", englishPtr, "")
            }
        }
        // Same tokens as the IBus engine (CHEWING_PLUS_IBUS_FEATURES).
        let features = Self.env("CHEWING_PLUS_FEATURES") ?? Self.env("CHEWING_PLUS_IBUS_FEATURES")
        if let features {
            if features.contains("emoji") { chewing_plus_set_emoji(session, 1) }
            if features.contains("calculator") { chewing_plus_set_calculator(session, 1) }
            if features.contains("space") { chewing_plus_set_spacing(session, 1, 1) }
            if features.contains("shortcode") {
                chewing_plus_set_emoji_shortcode(session, 1, ";;", 300)
            }
            if features.contains("english") {
                chewing_plus_set_english(session, 1, 1, "Control+Shift+Space")
                chewing_plus_set_english_double_tap(session, 1, 300)
            }
        }
        let userPtr = Unmanaged.passUnretained(self).toOpaque()
        var callbacks = chewing_plus_callbacks(commit: chewCommit, updated: nil, user: userPtr)
        commitHook = callbacks
        chewing_plus_set_callbacks(session, &callbacks)
    }

    fileprivate func insertCommitted(_ text: String) {
        lastClient?.insertText(text, replacementRange: NSRange(location: NSNotFound, length: NSNotFound))
    }

    private func publish(_ session: OpaquePointer) {
        var cursor = 0
        var hlStart = 0
        var hlLen = 0
        let raw = chewing_plus_preedit(session, &cursor, &hlStart, &hlLen)
        let text = raw.map { String(cString: $0) } ?? ""
        let selection = hlLen > 0
            ? NSRange(location: hlStart, length: hlLen)
            : NSRange(location: cursor, length: 0)
        let replace = NSRange(location: NSNotFound, length: NSNotFound)
        lastClient?.setMarkedText(text, selectionRange: selection, replacementRange: replace)
        let count = chewing_plus_candidate_count(session)
        if count > 0 {
            panel?.update()
            panel?.show(kIMKLocateCandidatesBelowHint)
        } else {
            panel?.hide()
        }
    }

    private func selectDigit(_ event: NSEvent, session: OpaquePointer) -> Bool {
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

    private func voiceStart() -> Bool {
        guard let session, !secure() else { return true }
        if voxkit_client_fd(voice) < 0 && voxkit_client_connect(voice, nil) != 0 {
            return true
        }
        voiceReq += 1
        var line = [CChar](repeating: 0, count: 512)
        let wrote = line.withUnsafeMutableBufferPointer { buffer -> Int32 in
            voxkit_proto_write_start(buffer.baseAddress, buffer.count, voiceReq,
                                     "zh", "s2twp", nil, nil, 0)
        }
        if wrote > 0 {
            line.withUnsafeBufferPointer { buffer in
                _ = voxkit_client_send(voice, buffer.baseAddress)
            }
            voiceActive = true
        }
        return true
    }

    private func voiceStop() -> Bool {
        guard voiceActive, let session else { return false }
        sendSimple("stop")
        let msg = UnsafeMutablePointer<voxkit_message>.allocate(capacity: 1)
        defer { msg.deallocate() }
        chewing_plus_mac_zero_message(msg)
        if voxkit_client_next(voice, msg, 200, nil, 0) == 1 && msg.pointee.type == VOXKIT_MSG_FINAL {
            chewing_plus_insert_text(session, chewing_plus_mac_message_text(msg))
            publish(session)
        }
        voiceActive = false
        return true
    }

    private func voiceCancel() -> Bool {
        sendSimple("cancel")
        voiceActive = false
        return true
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
            guard event.type == .keyDown,
                  let chars = event.charactersIgnoringModifiers,
                  let scalar = chars.unicodeScalars.first,
                  scalar.value > 0x20, scalar.value < 0x7F else {
                return false
            }
            key.ascii = scalar.value
        }
        return true
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
