import Carbon
import Cocoa
import InputMethodKit

let connection = "ChewingPlus_Connection"
guard let bundleID = Bundle.main.bundleIdentifier else {
    fatalError("missing bundle id")
}

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
        print("no input source for \(bundleID); log out and back in")
        return 1
    }
    for source in list {
        let idPtr = TISGetInputSourceProperty(source, kTISPropertyInputSourceID)
        let sourceID = idPtr.map {
            Unmanaged<CFString>.fromOpaque($0).takeUnretainedValue() as String
        } ?? "?"
        let enabled = TISEnableInputSource(source)
        print("enable \(sourceID): \(enabled == noErr ? "ok" : "error \(enabled)")")
    }
    return 0
}

if CommandLine.arguments.contains("--register") {
    exit(registerInputSource())
}

let server = IMKServer(name: connection, bundleIdentifier: bundleID)
NSApplication.shared.run()
_ = server
