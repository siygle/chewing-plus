import Carbon
import Cocoa
import InputMethodKit
import os.log

guard let bundleID = Bundle.main.bundleIdentifier else {
    fatalError("missing bundle id")
}
/// macOS 26 InputMethodKit only accepts a third-party connection named
/// "<bundle id>.Connection"; any other InputMethodConnectionName is refused
/// ("unrecognized 'InputMethodConnectionName' value") and the input method
/// never receives a key. Info.plist carries the same string.
let connection = bundleID + ".Connection"

/// `ChewingPlus --register`: register this bundle with Text Input Sources and
/// enable it, so it shows up without logging out. The installer runs this as
/// the console user. Prints what it did and exits.
func registerInputSource() -> Int32 {
    let url = Bundle.main.bundleURL as CFURL
    let status = TISRegisterInputSource(url)
    if status != noErr {
        print("TISRegisterInputSource failed: \(status)")
    }
    let filter = [kTISPropertyBundleID as String: bundleID] as CFDictionary
    guard let list = TISCreateInputSourceList(filter, true)?.takeRetainedValue()
        as? [TISInputSource], !list.isEmpty
    else {
        print("no input source for \(bundleID) at \(Bundle.main.bundlePath)")
        print("log out and back in; if it is still missing, open an issue with this output")
        return 1
    }
    var failed = false
    var sawMode = false
    for source in list {
        let idPtr = TISGetInputSourceProperty(source, kTISPropertyInputSourceID)
        let sourceID = idPtr.map {
            Unmanaged<CFString>.fromOpaque($0).takeUnretainedValue() as String
        } ?? "?"
        if sourceID == bundleID + ".Bopomofo" {
            sawMode = true
        }
        let enabled = TISEnableInputSource(source)
        failed = failed || enabled != noErr
        print("enable \(sourceID): \(enabled == noErr ? "ok" : "error \(enabled)")")
    }
    if !sawMode {
        print("input mode \(bundleID).Bopomofo is missing")
        return 1
    }
    return failed ? 1 : 0
}

// The voxkit LaunchAgent listens on this path (macos/launchd and
// pkg-scripts/agent.plist set VOXKIT_SOCKET). macOS starts the input method
// from launchd without that variable, so without this the client falls back
// to /tmp/voxkit-<uid>.sock and never reaches the daemon.
setenv("VOXKIT_SOCKET", "/tmp/chewing-plus-voxkit.sock", 0)

if CommandLine.arguments.contains("--register") {
    exit(registerInputSource())
}

let log = OSLog(subsystem: "im.chewing.inputmethod.ChewingPlus", category: "server")
let server = IMKServer(name: connection, bundleIdentifier: bundleID)
os_log("IMKServer %{public}@ for %{public}@: %{public}@", log: log, type: .default,
       connection, bundleID, server == nil ? "nil" : "up")
NSApplication.shared.run()
_ = server
