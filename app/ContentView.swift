import SwiftUI

struct ContentView: View {
    @StateObject private var manager = ExtensionManager()

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("macgip").font(.title)
            Text("Driver for wired GIP gamepads, compatible with Xbox controllers.")
            Text("Supported today: PowerA wired controller for Xbox Series X (USB 20d6:2007).")
                .font(.callout).foregroundStyle(.secondary)
            HStack {
                Button("Install driver") { manager.install() }.disabled(manager.busy)
                Button("Uninstall driver") { manager.uninstall() }.disabled(manager.busy)
            }
            Text(manager.status).font(.callout).textSelection(.enabled)
            Spacer()
            Text("Xbox is a trademark of the Microsoft group of companies. PowerA is a trademark of its owner. This app is not affiliated with either.")
                .font(.footnote).foregroundStyle(.secondary)
        }
        .padding(20)
    }
}
