import SwiftUI

@main
struct MacgipApp: App {
    var body: some Scene {
        WindowGroup("macgip") {
            ContentView()
                .frame(minWidth: 420, minHeight: 260)
        }
        .windowResizability(.contentSize)
    }
}
