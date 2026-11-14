import Cocoa
import InputMethodKit

let connection = "ChewingPlus_Connection"
guard let bundleID = Bundle.main.bundleIdentifier else {
    fatalError("missing bundle id")
}
let server = IMKServer(name: connection, bundleIdentifier: bundleID)
NSApplication.shared.run()
_ = server
