// Fails unless Text Input Sources lists the given input source id.
//
//   xcrun swift macos/check-tis.swift im.chewing.inputmethod.ChewingPlus.Bopomofo
//
// CI runs this after installing the pkg, so a bundle that macOS silently
// ignores (issue #7) turns the job red.
import Carbon
import Foundation

let wanted = CommandLine.arguments.count > 1
    ? CommandLine.arguments[1] : "im.chewing.inputmethod.ChewingPlus.Bopomofo"
let all = TISCreateInputSourceList(nil, true)?.takeRetainedValue() as? [TISInputSource] ?? []
var found = false
for source in all {
    guard let ptr = TISGetInputSourceProperty(source, kTISPropertyInputSourceID) else { continue }
    let id = Unmanaged<CFString>.fromOpaque(ptr).takeUnretainedValue() as String
    if id.contains("chewing") || id.contains("ChewingPlus") {
        print("tis: \(id)")
    }
    found = found || id == wanted
}
print("tis: \(all.count) input sources, \(wanted) \(found ? "present" : "MISSING")")
exit(found ? 0 : 1)
