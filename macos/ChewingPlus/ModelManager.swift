import Cocoa
import SwiftUI

// 「語音模型」: one card per catalog model (download / cancel / delete / use /
// verify), license consent for restricted models, store usage and free space.
// It talks to the voxkit daemon over the shared C client; when the daemon is
// not running it runs the installed `voxkit model … --json` CLI instead. Both
// paths produce the same JSON objects, decoded here with Foundation.

// MARK: - Data

struct ModelRow: Identifiable, Equatable {
    let id: String
    let title: String
    let purpose: String
    let blurb: String
    let license: String
    let restriction: String
    let bytes: Int64
    let sizeOnDisk: Int64
    let partialBytes: Int64
    let runtimeBytes: Int64
    let installed: Bool
    let active: Bool
    let restricted: Bool
    let licenseAccepted: Bool
    let recommended: Bool
    let selectable: Bool
    let removable: Bool
    let downloading: Bool
    let borrowed: Bool

    init(_ json: [String: Any]) {
        func text(_ key: String) -> String { json[key] as? String ?? "" }
        func number(_ key: String) -> Int64 { (json[key] as? NSNumber)?.int64Value ?? 0 }
        func flag(_ key: String) -> Bool { (json[key] as? NSNumber)?.boolValue ?? false }
        id = text("id")
        let named = text("title")
        title = named.isEmpty ? id : named
        let purposeText = text("purpose")
        purpose = purposeText.isEmpty ? text("kind") : purposeText
        blurb = text("blurb")
        license = text("license")
        restriction = text("restriction")
        bytes = number("bytes")
        sizeOnDisk = number("size_on_disk")
        partialBytes = number("partial_bytes")
        runtimeBytes = number("runtime_bytes")
        installed = flag("installed")
        active = flag("active")
        restricted = flag("restricted")
        licenseAccepted = flag("license_accepted") || !restricted
        recommended = flag("recommended")
        selectable = json["selectable"] == nil ? purpose != "vad" : flag("selectable")
        removable = json["removable"] == nil ? installed : flag("removable")
        downloading = flag("downloading")
        borrowed = !text("source").isEmpty && text("source") != "store"
    }

    var purposeLabel: String {
        switch purpose {
        case "asr": return "語音辨識"
        case "mt": return "翻譯"
        case "vad": return "語音偵測"
        default: return purpose
        }
    }
}

struct Transfer: Equatable {
    var model = ""
    var state = ""
    var received: Int64 = 0
    var bytes: Int64 = 0
    var message = ""

    var busy: Bool { state == "downloading" || state == "verifying" }
    var fraction: Double { bytes > 0 ? min(1, Double(received) / Double(bytes)) : 0 }
}

func formatBytes(_ value: Int64) -> String {
    ByteCountFormatter.string(fromByteCount: value, countStyle: .file)
}

// MARK: - Backend

/// Talks JSON to the running daemon, or to the offline CLI.
final class VoxkitBackend {
    private let client = UnsafeMutablePointer<voxkit_client>.allocate(capacity: 1)
    private var req: UInt64 = 100
    private var offlineJob: Process?

    init() {
        voxkit_client_init(client)
    }

    var support: URL {
        FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Application Support/ChewingPlus")
    }

    var storeDir: URL { support.appendingPathComponent("models") }

    var cliPath: String? {
        let candidates = [support.appendingPathComponent("voxkit").path,
                          "/Library/Application Support/ChewingPlus/voxkit"]
        return candidates.first { FileManager.default.isExecutableFile(atPath: $0) }
    }

    var online: Bool {
        if voxkit_client_fd(client) >= 0 { return true }
        return voxkit_client_connect(client, nil) == 0
    }

    var offlineBusy: Bool { offlineJob?.isRunning ?? false }

    /// One request/reply over the socket. nil when the daemon is not there.
    func request(_ body: [String: Any], timeout: Int32 = 1500) -> [String: Any]? {
        guard online else { return nil }
        var payload = body
        payload["v"] = 1
        req += 1
        payload["req"] = req
        guard let data = try? JSONSerialization.data(withJSONObject: payload),
              let text = String(data: data, encoding: .utf8)
        else { return nil }
        if (text + "\n").withCString({ voxkit_client_send(client, $0) }) != 0 {
            voxkit_client_close(client)
            return nil
        }
        var msg = chewing_plus_mac_blank_message()
        let rc = voxkit_client_next(client, &msg, timeout, nil, 0)
        // -1 with the socket still open is a line the C parser does not know
        // (a newer daemon); the raw JSON is still usable.
        if rc == 0 || voxkit_client_fd(client) < 0 { return nil }
        return decode(String(cString: voxkit_client_last_line(client)))
    }

    func decode(_ line: String) -> [String: Any]? {
        guard let data = line.data(using: .utf8),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        else { return nil }
        return object
    }

    private func cliProcess(_ args: [String]) -> Process? {
        guard let path = cliPath else { return nil }
        let process = Process()
        process.executableURL = URL(fileURLWithPath: path)
        process.arguments = ["model"] + args + ["--json"]
        var env = ProcessInfo.processInfo.environment
        env["VOXKIT_MODELS_DIR"] = storeDir.path
        env["VOXKIT_SOCKET"] = "/tmp/chewing-plus-voxkit.sock"
        if env["VOXKIT_MODEL"] == nil { env["VOXKIT_MODEL"] = "qwen3-asr-0.6b" }
        process.environment = env
        return process
    }

    /// Short offline commands (list, remove, select): every JSON line printed.
    func runOffline(_ args: [String]) -> [[String: Any]] {
        guard let process = cliProcess(args) else { return [] }
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = FileHandle.nullDevice
        do { try process.run() } catch { return [] }
        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()
        let text = String(data: data, encoding: .utf8) ?? ""
        return text.split(separator: "\n").compactMap { decode(String($0)) }
    }

    /// Long offline commands (install, verify): lines arrive on the main queue.
    func startOffline(_ args: [String], line: @escaping ([String: Any]) -> Void,
                      done: @escaping () -> Void) -> Bool {
        guard offlineJob?.isRunning != true, let process = cliProcess(args) else { return false }
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = FileHandle.nullDevice
        var pending = Data()
        pipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let chunk = handle.availableData
            if chunk.isEmpty {
                handle.readabilityHandler = nil
                return
            }
            pending.append(chunk)
            while let newline = pending.firstIndex(of: 0x0A) {
                let lineData = pending[pending.startIndex..<newline]
                pending.removeSubrange(pending.startIndex...newline)
                if let text = String(data: lineData, encoding: .utf8),
                   let object = self?.decode(text) {
                    DispatchQueue.main.async { line(object) }
                }
            }
        }
        process.terminationHandler = { _ in
            DispatchQueue.main.async { done() }
        }
        do { try process.run() } catch { return false }
        offlineJob = process
        return true
    }

    /// Stops an offline download. The partial file stays for a later resume.
    func cancelOffline() {
        offlineJob?.terminate()
    }
}

// MARK: - Model

final class ModelManagerModel: ObservableObject {
    @Published var rows: [ModelRow] = []
    @Published var consent: Set<String> = []
    @Published var transfer = Transfer()
    @Published var online = false
    @Published var storePath = ""
    @Published var storeBytes: Int64 = 0
    @Published var freeBytes: Int64 = 0
    @Published var notice = ""
    @Published var noticeIsError = false

    let backend = VoxkitBackend()
    private var timer: Timer?
    private var lastState = ""

    func start() {
        refresh()
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in
            self?.poll()
        }
    }

    func stop() {
        timer?.invalidate()
        timer = nil
    }

    func refresh() {
        let reply: [String: Any]?
        if let live = backend.request(["type": "model.list"]) {
            online = true
            reply = live
        } else {
            online = false
            reply = backend.runOffline(["list"]).first { $0["type"] as? String == "model.list" }
            if reply == nil {
                show(backend.cliPath == nil
                     ? "找不到 voxkit。請重新安裝新酷音 Plus。"
                     : "讀不到模型列表。", error: true)
            }
        }
        guard let list = reply else {
            rows = []
            return
        }
        rows = (list["models"] as? [[String: Any]] ?? []).map(ModelRow.init)
        storePath = list["store"] as? String ?? backend.storeDir.path
        storeBytes = (list["store_bytes"] as? NSNumber)?.int64Value ?? 0
        freeBytes = (list["free_bytes"] as? NSNumber)?.int64Value ?? 0
        if !online && !backend.offlineBusy && transfer.busy {
            transfer = Transfer()
        }
    }

    private func poll() {
        guard online else { return }
        guard let reply = backend.request(["type": "model.progress"], timeout: 300) else {
            online = false
            refresh()
            return
        }
        applyProgress(reply)
    }

    private func applyProgress(_ reply: [String: Any]) {
        var next = Transfer()
        next.model = reply["model"] as? String ?? ""
        next.state = reply["state"] as? String ?? ""
        next.received = (reply["received"] as? NSNumber)?.int64Value ?? 0
        next.bytes = (reply["bytes"] as? NSNumber)?.int64Value ?? 0
        next.message = reply["message"] as? String ?? ""
        if next != transfer { transfer = next }
        let state = next.state
        if state != lastState {
            if state == "done" || state == "cancelled" || state == "error" {
                if state == "error" {
                    show(next.message.isEmpty ? "下載失敗。" : next.message, error: true)
                } else if state == "done", !next.message.isEmpty {
                    show(verifyWord(next.message, model: next.model), error: next.message == "corrupt")
                }
                refresh()
            }
            lastState = state
        }
    }

    private func verifyWord(_ result: String, model: String) -> String {
        switch result {
        case "ok": return "\(model) 檢查通過。"
        case "missing": return "\(model) 尚未下載。"
        case "corrupt": return "\(model) 檔案損毀，請刪除後重新下載。"
        default: return result
        }
    }

    func show(_ text: String, error: Bool = false) {
        notice = text
        noticeIsError = error
    }

    private func handleError(_ reply: [String: Any]) -> Bool {
        guard reply["type"] as? String == "error" else { return false }
        let code = reply["code"] as? String ?? ""
        let message = reply["message"] as? String ?? code
        switch code {
        case "license-required": show("請先勾選同意授權條款再下載。", error: true)
        case "no-space": show("磁碟空間不足：\(message)", error: true)
        case "in-use": show("模型正在使用中，請稍後再刪除。", error: true)
        case "daemon-running": show("語音服務剛啟動，請按「重新整理」。", error: true)
        default: show(message, error: true)
        }
        return true
    }

    func install(_ row: ModelRow) {
        let accept = row.restricted && consent.contains(row.id)
        show("")
        if online {
            var body: [String: Any] = ["type": "model.install", "model": row.id]
            if accept { body["accept_license"] = true }
            guard let reply = backend.request(body) else {
                show("連不到語音服務。", error: true)
                return
            }
            if !handleError(reply) { applyProgress(reply) }
            return
        }
        var args = ["install", row.id]
        if accept { args.append("--accept-license") }
        transfer = Transfer(model: row.id, state: "downloading", received: row.partialBytes,
                            bytes: row.bytes, message: "")
        let started = backend.startOffline(args, line: { [weak self] object in
            guard let self = self else { return }
            if !self.handleError(object), object["type"] as? String == "model.progress" {
                self.applyProgress(object)
            }
        }, done: { [weak self] in
            guard let self = self else { return }
            if self.transfer.busy { self.transfer.state = "cancelled" }
            self.lastState = ""
            self.refresh()
        })
        if !started {
            transfer = Transfer()
            show(backend.offlineBusy ? "已有下載進行中。" : "無法啟動 voxkit。", error: true)
        }
    }

    func cancel() {
        if online {
            if let reply = backend.request(["type": "model.cancel"]) { applyProgress(reply) }
        } else {
            backend.cancelOffline()
        }
    }

    func remove(_ row: ModelRow) {
        show("")
        if online {
            guard let reply = backend.request(["type": "model.remove", "model": row.id]) else { return }
            if !handleError(reply) { show("已刪除 \(row.title)。") }
        } else {
            for line in backend.runOffline(["remove", row.id]) where !handleError(line) {
                show("已刪除 \(row.title)。")
            }
        }
        refresh()
    }

    func use(_ row: ModelRow) {
        show("")
        if online {
            guard let reply = backend.request(["type": "model.select", "model": row.id]) else { return }
            if !handleError(reply) { show("已改用 \(row.title)。") }
        } else {
            for line in backend.runOffline(["select", row.id]) where !handleError(line) {
                show("已改用 \(row.title)（語音服務啟動後生效）。")
            }
        }
        refresh()
    }

    func verify(_ row: ModelRow) {
        show("")
        if online {
            guard let reply = backend.request(["type": "model.verify", "model": row.id]) else { return }
            if !handleError(reply) { applyProgress(reply) }
            return
        }
        transfer = Transfer(model: row.id, state: "verifying", received: 0, bytes: 0, message: "")
        let started = backend.startOffline(["verify", row.id], line: { [weak self] object in
            guard let self = self else { return }
            if self.handleError(object) { return }
            if let result = object["result"] as? String {
                self.show(self.verifyWord(result, model: row.id), error: result != "ok")
            }
        }, done: { [weak self] in
            self?.transfer = Transfer()
            self?.refresh()
        })
        if !started { transfer = Transfer() }
    }

    func revealStore() {
        let url = URL(fileURLWithPath: storePath.isEmpty ? backend.storeDir.path : storePath)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        NSWorkspace.shared.activateFileViewerSelecting([url])
    }
}

// MARK: - Views

struct ModelManagerView: View {
    @ObservedObject var model: ModelManagerModel

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            header
                .padding(16)
            Divider()
            ScrollView {
                VStack(alignment: .leading, spacing: 24) {
                    modelSection(
                        title: "語音辨識模型",
                        subtitle: selectedLine(
                            in: speechRows,
                            empty: "尚未選擇語音辨識模型"),
                        rows: speechRows)
                    modelSection(
                        title: "翻譯模型",
                        subtitle: selectedLine(
                            in: translateRows,
                            empty: "尚未選擇翻譯模型"),
                        rows: translateRows)
                }
                .padding(16)
            }
            if !model.notice.isEmpty {
                Divider()
                Text(model.notice)
                    .font(.callout)
                    .foregroundColor(model.noticeIsError ? Color.red : Color.secondary)
                    .padding(.horizontal, 16)
                    .padding(.vertical, 8)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
        .frame(minWidth: 560, minHeight: 480)
    }

    /// Speech recognition and VAD together; translation in its own section.
    private var speechRows: [ModelRow] {
        model.rows.filter { $0.purpose != "mt" }
    }

    private var translateRows: [ModelRow] {
        model.rows.filter { $0.purpose == "mt" }
    }

    private func selectedLine(in rows: [ModelRow], empty: String) -> String {
        guard let row = rows.first(where: { $0.active }) else { return empty }
        if row.installed {
            return "目前使用：\(row.title)"
        }
        return "目前選擇：\(row.title)（尚未下載）"
    }

    @ViewBuilder
    private func modelSection(title: String, subtitle: String,
                              rows: [ModelRow]) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.title3).bold()
                Text(subtitle).font(.callout).foregroundColor(.secondary)
            }
            if rows.isEmpty {
                Text("這個版本沒有這類模型。")
                    .font(.callout)
                    .foregroundColor(.secondary)
            } else {
                ForEach(rows) { row in
                    ModelCard(row: row, model: model)
                }
            }
        }
    }

    private var header: some View {
        HStack(alignment: .top) {
            VStack(alignment: .leading, spacing: 4) {
                Text("語音模型").font(.title2).bold()
                Text("已使用 \(formatBytes(model.storeBytes)) · 可用空間 \(formatBytes(model.freeBytes))")
                    .font(.callout)
                HStack(spacing: 6) {
                    Circle()
                        .fill(model.online ? Color.green : Color.orange)
                        .frame(width: 8, height: 8)
                    Text(model.online ? "語音服務執行中" : "語音服務未執行（離線管理）")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }
            Spacer()
            Button("在 Finder 顯示") { model.revealStore() }
            Button("重新整理") { model.refresh() }
        }
    }
}

struct ModelCard: View {
    let row: ModelRow
    @ObservedObject var model: ModelManagerModel
    @State private var confirmDelete = false

    private var transferring: Bool { model.transfer.model == row.id && model.transfer.busy }
    private var downloading: Bool {
        (transferring && model.transfer.state == "downloading") || (row.downloading && !transferring && model.online)
    }
    private var someoneBusy: Bool { model.transfer.busy || model.backend.offlineBusy }
    private var consentGiven: Bool { row.licenseAccepted || model.consent.contains(row.id) }

    var body: some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 8) {
                titleLine
                Text(sizeLine).font(.callout)
                if !row.license.isEmpty {
                    Text("授權：\(row.license)").font(.caption).foregroundColor(.secondary)
                }
                if !row.blurb.isEmpty {
                    Text(row.blurb).font(.caption).foregroundColor(.secondary).lineLimit(3)
                }
                if row.restricted && !row.installed {
                    restrictionBox
                }
                if transferring || downloading {
                    progressLine
                }
                buttons
            }
            .padding(6)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .alert("刪除 \(row.title)？", isPresented: $confirmDelete) {
            Button("刪除", role: .destructive) { model.remove(row) }
            Button("取消", role: .cancel) {}
        } message: {
            Text("會刪除 \(formatBytes(row.sizeOnDisk)) 的模型檔案，之後需要時可重新下載。")
        }
    }

    private var titleLine: some View {
        HStack(spacing: 6) {
            Text(row.title).font(.headline)
            badge(row.purposeLabel, .blue)
            if row.recommended { badge("建議", .green) }
            if row.active && row.installed { badge("使用中", .accentColor) }
            Spacer()
            Text(statusText).font(.callout).foregroundColor(.secondary)
        }
    }

    private func badge(_ text: String, _ color: Color) -> some View {
        Text(text)
            .font(.caption)
            .padding(.horizontal, 6)
            .padding(.vertical, 1)
            .background(color.opacity(0.15))
            .foregroundColor(color)
            .cornerRadius(4)
    }

    private var statusText: String {
        if transferring && model.transfer.state == "verifying" { return "檢查中…" }
        if downloading { return "下載中" }
        if row.installed { return row.borrowed ? "已安裝（共用，唯讀）" : "已安裝" }
        if row.partialBytes > 0 { return "已下載 \(formatBytes(row.partialBytes))，可續傳" }
        return "未下載"
    }

    private var sizeLine: String {
        var parts = ["下載 \(formatBytes(row.bytes))"]
        if row.runtimeBytes > 0 { parts.append("執行時約 \(formatBytes(row.runtimeBytes)) 記憶體") }
        if row.installed && row.sizeOnDisk > 0 { parts.append("佔用 \(formatBytes(row.sizeOnDisk))") }
        return parts.joined(separator: " · ")
    }

    private var restrictionBox: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(row.restriction.isEmpty ? "此模型有使用限制，下載前請先閱讀授權條款。" : row.restriction)
                .font(.callout)
                .foregroundColor(.orange)
            if row.licenseAccepted {
                Text("已同意授權條款。").font(.caption).foregroundColor(.secondary)
            } else {
                Toggle("我已閱讀並同意上述授權條款", isOn: Binding(
                    get: { model.consent.contains(row.id) },
                    set: { on in
                        if on { model.consent.insert(row.id) } else { model.consent.remove(row.id) }
                    }))
                    .toggleStyle(.checkbox)
            }
        }
    }

    private var progressLine: some View {
        VStack(alignment: .leading, spacing: 2) {
            if model.transfer.state == "verifying" {
                ProgressView().progressViewStyle(.linear)
            } else {
                ProgressView(value: model.transfer.fraction)
                Text("\(formatBytes(model.transfer.received)) / \(formatBytes(model.transfer.bytes))")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
        }
    }

    private var buttons: some View {
        HStack {
            if downloading {
                Button("取消下載") { model.cancel() }
            } else if !row.installed {
                Button(row.partialBytes > 0 ? "繼續下載" : "下載") { model.install(row) }
                    .disabled(someoneBusy || !consentGiven)
            }
            if row.installed {
                if row.selectable && !row.active {
                    Button("使用") { model.use(row) }.disabled(someoneBusy)
                }
                Button("檢查檔案") { model.verify(row) }.disabled(someoneBusy)
                if row.removable {
                    Button("刪除") { confirmDelete = true }.disabled(transferring)
                }
            }
            Spacer()
        }
    }
}

// MARK: - Window

/// Opened from the input menu (「語音模型…」) or standalone with
/// `ChewingPlus --models` (the 「新酷音 Plus 語音模型」 app).
final class ModelManagerWindow: NSObject, NSWindowDelegate {
    static let shared = ModelManagerWindow()

    private let model = ModelManagerModel()
    private var window: NSWindow?
    var quitOnClose = false

    func show() {
        let target = window ?? makeWindow()
        window = target
        model.start()
        if !target.isVisible { target.center() }
        target.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    private func makeWindow() -> NSWindow {
        let made = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 640, height: 640),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false)
        made.title = "語音模型"
        made.isReleasedWhenClosed = false
        made.contentView = NSHostingView(rootView: ModelManagerView(model: model))
        made.delegate = self
        return made
    }

    func windowWillClose(_: Notification) {
        model.stop()
        if quitOnClose {
            NSApp.terminate(nil)
        }
    }
}
