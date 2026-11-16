import Cocoa

/// AppKit preferences for the voxkit model catalog. The window only calls
/// the shared C client. It does not download bytes itself.
final class ModelPreferences: NSObject, NSTableViewDataSource, NSTableViewDelegate {
    static let shared = ModelPreferences()

    private let window: NSWindow
    private let table = NSTableView()
    private let statusLabel = NSTextField(wrappingLabelWithString: "")
    private let progress = NSProgressIndicator()
    private let voice = UnsafeMutablePointer<voxkit_client>.allocate(capacity: 1)
    private var rows: [voxkit_model_info] = []
    private var timer: Timer?
    private var req: UInt64 = 20
    private var lastState = ""

    private override init() {
        window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 640, height: 420),
            styleMask: [.titled, .closable, .miniaturizable],
            backing: .buffered,
            defer: false)
        super.init()
        voxkit_client_init(voice)
        window.title = "語音模型"
        window.isReleasedWhenClosed = false
        build()
    }

    func show() {
        refresh()
        window.center()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 0.4, repeats: true) { [weak self] _ in
            self?.poll()
        }
    }

    private func build() {
        guard let content = window.contentView else { return }
        let scroll = NSScrollView(frame: NSRect(x: 20, y: 150, width: 600, height: 230))
        scroll.hasVerticalScroller = true
        scroll.borderType = .bezelBorder
        table.frame = scroll.bounds
        table.headerView = NSTableHeaderView()
        for (id, title, width) in [("id", "模型", 180.0), ("kind", "類型", 60.0),
                                   ("state", "狀態", 110.0), ("bytes", "大小", 120.0)] {
            let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier(id))
            column.title = title
            column.width = width
            table.addTableColumn(column)
        }
        table.dataSource = self
        table.delegate = self
        scroll.documentView = table
        content.addSubview(scroll)

        statusLabel.frame = NSRect(x: 20, y: 110, width: 600, height: 36)
        statusLabel.maximumNumberOfLines = 2
        content.addSubview(statusLabel)

        progress.frame = NSRect(x: 20, y: 88, width: 600, height: 16)
        progress.isIndeterminate = false
        progress.minValue = 0
        progress.maxValue = 1
        content.addSubview(progress)

        let titles = ["重新整理", "下載", "取消", "刪除", "使用"]
        let actions: [Selector] = [#selector(refreshClicked(_:)), #selector(installClicked(_:)),
                                   #selector(cancelClicked(_:)), #selector(removeClicked(_:)),
                                   #selector(selectClicked(_:))]
        for index in 0..<titles.count {
            let button = NSButton(frame: NSRect(x: 20 + index * 120, y: 24, width: 110, height: 32))
            button.title = titles[index]
            button.bezelStyle = .rounded
            button.target = self
            button.action = actions[index]
            content.addSubview(button)
        }
    }

    @objc private func refreshClicked(_: Any?) { refresh() }
    @objc private func installClicked(_: Any?) { mutate(.install) }
    @objc private func cancelClicked(_: Any?) {
        _ = ensure()
        var msg = chewing_plus_mac_blank_message()
        _ = voxkit_client_model_cancel(voice, nextReq(), &msg, 500, nil, 0)
        apply(msg)
        refresh()
    }
    @objc private func removeClicked(_: Any?) { mutate(.remove) }
    @objc private func selectClicked(_: Any?) { mutate(.select) }

    private enum Action { case install, remove, select }

    private func mutate(_ action: Action) {
        guard let id = selectedId() else { return }
        guard ensure() else { return }
        var msg = chewing_plus_mac_blank_message()
        let rc: Int32
        switch action {
        case .install:
            rc = id.withCString { voxkit_client_model_install(voice, nextReq(), $0, &msg, 1000, nil, 0) }
        case .remove:
            rc = id.withCString { voxkit_client_model_remove(voice, nextReq(), $0, &msg, 1000, nil, 0) }
        case .select:
            rc = id.withCString { voxkit_client_model_select(voice, nextReq(), $0, &msg, 1000, nil, 0) }
        }
        if rc != 0 {
            statusLabel.stringValue = "操作失敗。請確認 voxkit 正在執行。"
            return
        }
        apply(msg)
        if action != .install {
            refresh()
        }
    }

    private func refresh() {
        guard ensure() else {
            rows = []
            table.reloadData()
            statusLabel.stringValue = "連不到 voxkit 語音服務。請確認它已安裝並在執行。"
            return
        }
        var storage = [voxkit_model_info](repeating: chewing_plus_mac_blank_model(), count: 8)
        var count: Int = 0
        var summary = chewing_plus_mac_blank_message()
        let rc = storage.withUnsafeMutableBufferPointer { buffer -> Int32 in
            voxkit_client_model_list(voice, nextReq(), buffer.baseAddress, buffer.count,
                                     &count, &summary, 1000, nil, 0)
        }
        if rc != 0 {
            statusLabel.stringValue = "讀不到模型列表。"
            return
        }
        rows = Array(storage.prefix(count))
        table.reloadData()
        apply(summary)
    }

    private func poll() {
        guard window.isVisible, voxkit_client_fd(voice) >= 0 else { return }
        var msg = chewing_plus_mac_blank_message()
        if voxkit_client_model_progress(voice, nextReq(), &msg, 200, nil, 0) != 0 {
            return
        }
        apply(msg)
        let state = String(cString: chewing_plus_mac_state(&msg))
        if state != lastState &&
            (state == "done" || state == "cancelled" || state == "error") {
            lastState = state
            refresh()
            return
        }
        lastState = state
    }

    private func apply(_ msg: voxkit_message) {
        var copy = msg
        let daemon = String(cString: chewing_plus_mac_daemon(&copy))
        let state = String(cString: chewing_plus_mac_state(&copy))
        let store = String(cString: chewing_plus_mac_store(&copy))
        let active = String(cString: chewing_plus_mac_active(&copy))
        let note = String(cString: chewing_plus_mac_note(&copy))
        let used = ByteCountFormatter.string(fromByteCount: Int64(copy.store_bytes), countStyle: .file)
        var line = "服務 \(daemon.isEmpty ? "執行中" : daemon) · \(state.isEmpty ? "閒置" : state) · 已用 \(used)"
        if !active.isEmpty { line += " · 目前 \(active)" }
        if !store.isEmpty { line += "\n\(store)" }
        if !note.isEmpty { line += " · \(note)" }
        statusLabel.stringValue = line
        if copy.bytes > 0 {
            progress.doubleValue = Double(copy.received) / Double(copy.bytes)
        } else {
            progress.doubleValue = 0
        }
    }

    private func ensure() -> Bool {
        if voxkit_client_fd(voice) >= 0 { return true }
        return voxkit_client_connect(voice, nil) == 0
    }

    private func nextReq() -> UInt64 {
        req += 1
        return req
    }

    private func selectedId() -> String? {
        let row = table.selectedRow
        guard row >= 0, row < rows.count else { return nil }
        var info = rows[row]
        return String(cString: chewing_plus_mac_model_id(&info))
    }

    func numberOfRows(in _: NSTableView) -> Int { rows.count }

    func tableView(_: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        let cell = NSTextField(labelWithString: "")
        var info = rows[row]
        switch tableColumn?.identifier.rawValue {
        case "id":
            cell.stringValue = String(cString: chewing_plus_mac_model_id(&info))
        case "kind":
            cell.stringValue = String(cString: chewing_plus_mac_model_kind(&info))
        case "state":
            cell.stringValue = info.installed != 0 ? (info.active != 0 ? "使用中" : "已安裝") : "未安裝"
        default:
            cell.stringValue = ByteCountFormatter.string(fromByteCount: Int64(info.bytes), countStyle: .file)
        }
        return cell
    }
}
