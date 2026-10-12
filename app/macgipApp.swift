import SwiftUI

@main
struct MacgipApp: App {
    var body: some Scene {
        WindowGroup("macgip") {
            ContentView()
                .frame(minWidth: 520, minHeight: 420)
        }
        .windowResizability(.contentSize)
    }
}
