// gc_watch: print what the Game Controller framework sees. Build: swiftc -O gc_watch.swift -o gc_watch
import Foundation
import GameController

let secs = CommandLine.arguments.count > 1 ? Double(CommandLine.arguments[1]) ?? 20 : 20
var seen = 0
func attach(_ c: GCController) {
    print("controller: \(c.vendorName ?? "?") category=\(c.productCategory) extended=\(c.extendedGamepad != nil) micro=\(c.microGamepad != nil)")
    guard let g = c.extendedGamepad else { return }
    g.valueChangedHandler = { _, element in
        seen += 1
        if let b = element as? GCControllerButtonInput { print("  button \(b.localizedName ?? "?") = \(b.value)") }
        else if let d = element as? GCControllerDirectionPad { print("  dpad \(d.localizedName ?? "?") = (\(d.xAxis.value), \(d.yAxis.value))") }
        else if let a = element as? GCControllerAxisInput { print("  axis \(a.localizedName ?? "?") = \(a.value)") }
    }
}
NotificationCenter.default.addObserver(forName: .GCControllerDidConnect, object: nil, queue: .main) { n in
    if let c = n.object as? GCController { attach(c) }
}
GCController.controllers().forEach(attach)
print("controllers at start: \(GCController.controllers().count)")
RunLoop.main.run(until: Date(timeIntervalSinceNow: secs))
print("done, \(seen) element changes")
