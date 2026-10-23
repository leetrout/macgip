import SwiftUI

struct ContentView: View {
    @StateObject private var manager = ExtensionManager()
    @StateObject private var monitor = ControllerMonitor()

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("macgip").font(.title)
            Text("Driver for wired GIP gamepads, compatible with Xbox controllers.")
            Text("Supported today: PowerA wired controller for Xbox Series X (USB 20d6:2007), PDP Afterglow Wave Wired Controller for Xbox (USB 0e6f:0338).")
                .font(.callout).foregroundStyle(.secondary)
            HStack {
                Button("Install driver") { manager.install() }.disabled(manager.busy)
                Button("Uninstall driver") { manager.uninstall() }.disabled(manager.busy)
            }
            Text(manager.status).font(.callout).textSelection(.enabled)
            Divider()
            Text("Game Controller framework: \(monitor.name)").font(.headline)
            ForEach(monitor.lines, id: \.self) { Text($0).font(.system(.body, design: .monospaced)) }
            Spacer()
            Text("Xbox is a trademark of the Microsoft group of companies. PowerA and PDP are trademarks of their owners. This app is not affiliated with any of them.")
                .font(.footnote).foregroundStyle(.secondary)
        }
        .padding(20)
    }
}
