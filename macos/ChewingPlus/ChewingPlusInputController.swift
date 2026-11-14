import Cocoa
import InputMethodKit

/// Native InputMethodKit controller. Drawing stays in AppKit / IMK.
/// Composition is libchewing-plus-core. Voice is a version-1 client of the
/// voxkit launchd helper. This process does not load a model.
final class ChewingPlusInputController: IMKInputController {
    private var session: OpaquePointer?
    private var voice = UnsafeMutablePointer<voxkit_client>.allocate(capacity: 1)
    private var voiceActive = false
    private var voiceReq: UInt64 = 0

    override init!(server: IMKServer!, delegate: Any!, client: Any!) {
        super.init(server: server, delegate: delegate, client: client)
        voxkit_client_init(voice)
        let sys = Self.env("CHEWING_PATH")
        let user = Self.env("CHEWING_USER_PATH")
        session = sys.withOptionalCString { sysPtr in
            user.withOptionalCString { userPtr in
                chewing_plus_new(sysPtr, userPtr)
            }
        }
        guard let session else { return }
        let emoji = Self.env("CHEWING_PLUS_EMOJI")
        let english = Self.env("CHEWING_PLUS_ENGLISH")
        emoji.withOptionalCString { emojiPtr in
            english.withOptionalCString { englishPtr in
                chewing_plus_set_data_paths(session, emojiPtr, "", englishPtr, "")
            }
        }
        guard let features = Self.env("CHEWING_PLUS_FEATURES") else { return }
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

    deinit {
        voxkit_client_close(voice)
        voice.deallocate()
        chewing_plus_free(session)
    }

    override func recognizedEvents(_ sender: Any!) -> Int {
        let mask = NSEvent.EventTypeMask.keyDown.union(.keyUp).union(.flagsChanged)
        return Int(mask.rawValue)
    }

    override func handle(_ event: NSEvent!, client sender: Any!) -> Bool {
        guard let event, let session else { return false }
        let textClient = sender as? IMKTextInput
        if event.keyCode == 0x3D {
            let released = event.type == .keyUp
                || (event.type == .flagsChanged && !event.modifierFlags.contains(.option))
            return released ? voiceStop(client: textClient) : voiceStart()
        }
        if voiceActive && event.type == .keyDown {
            _ = voiceCancel()
        }
        var key = chewing_plus_key_event(key: 0, ascii: 0, mods: 0, keycode: Int32(event.keyCode), time_ms: -1)
        if event.modifierFlags.contains(.shift) { key.mods |= UInt32(CHEWING_PLUS_MOD_SHIFT) }
        if event.modifierFlags.contains(.control) { key.mods |= UInt32(CHEWING_PLUS_MOD_CTRL) }
        if event.modifierFlags.contains(.option) { key.mods |= UInt32(CHEWING_PLUS_MOD_ALT) }
        if event.type == .keyUp { key.mods |= UInt32(CHEWING_PLUS_MOD_RELEASE) }
        if !Self.fill(&key, from: event) { return false }
        _ = chewing_plus_key(session, &key)
        publish(session, to: textClient)
        return true
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
        case 60: key.key = UInt32(CHEWING_PLUS_KEY_SHIFT_R)
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

    private func publish(_ session: OpaquePointer, to client: IMKTextInput?) {
        guard let client else { return }
        var cursor = 0
        var hlStart = 0
        var hlLen = 0
        guard let raw = chewing_plus_preedit(session, &cursor, &hlStart, &hlLen) else { return }
        let text = String(cString: raw)
        let caret = NSRange(location: cursor, length: 0)
        let replace = NSRange(location: NSNotFound, length: NSNotFound)
        client.setMarkedText(text, selectionRange: caret, replacementRange: replace)
    }

    private func voiceStart() -> Bool {
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

    private func voiceStop(client: IMKTextInput?) -> Bool {
        guard voiceActive, let session else { return false }
        sendSimple("stop")
        var msg = voxkit_message()
        if voxkit_client_next(voice, &msg, 200, nil, 0) == 1 && msg.type == VOXKIT_MSG_FINAL {
            withUnsafePointer(to: msg.text) { pointer in
                pointer.withMemoryRebound(to: CChar.self, capacity: 1) { cString in
                    chewing_plus_insert_text(session, cString)
                }
            }
            publish(session, to: client)
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

    private static func env(_ name: String) -> String? {
        guard let raw = getenv(name) else { return nil }
        let value = String(cString: raw)
        return value.isEmpty ? nil : value
    }
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
