import Foundation
import GameController

/// Mirrors the first extended gamepad the Game Controller framework reports.
/// Polls the controller list on a main-thread timer; no values cross an isolation boundary.
@MainActor
final class ControllerMonitor: ObservableObject {
    @Published var name = "No controller"
    @Published var lines: [String] = []
    private var attached: GCController?
    private var timer: Timer?

    init() {
        timer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { _ in
            MainActor.assumeIsolated { ControllerMonitor.shared?.poll() }
        }
        ControllerMonitor.shared = self
        poll()
    }

    nonisolated(unsafe) static var shared: ControllerMonitor?

    private func poll() {
        let current = GCController.controllers().first
        if current !== attached {
            attached = current
            guard let c = current, let g = c.extendedGamepad else {
                name = "No controller"; lines = []; return
            }
            name = "\(c.vendorName ?? "?") [\(c.productCategory)]"
            g.valueChangedHandler = { g, _ in
                MainActor.assumeIsolated { ControllerMonitor.shared?.render(g) }   // GC calls on the main queue
            }
            render(g)
        }
    }

    private func f(_ v: Float) -> String { String(format: "%.2f", v) }

    private func render(_ g: GCExtendedGamepad) {
        lines = [
            "A \(f(g.buttonA.value))  B \(f(g.buttonB.value))  X \(f(g.buttonX.value))  Y \(f(g.buttonY.value))",
            "LB \(f(g.leftShoulder.value))  RB \(f(g.rightShoulder.value))  LT \(f(g.leftTrigger.value))  RT \(f(g.rightTrigger.value))",
            "D-pad (\(f(g.dpad.xAxis.value)), \(f(g.dpad.yAxis.value)))",
            "Left stick (\(f(g.leftThumbstick.xAxis.value)), \(f(g.leftThumbstick.yAxis.value)))  click \(f(g.leftThumbstickButton?.value ?? 0))",
            "Right stick (\(f(g.rightThumbstick.xAxis.value)), \(f(g.rightThumbstick.yAxis.value)))  click \(f(g.rightThumbstickButton?.value ?? 0))",
            "Menu \(f(g.buttonMenu.value))  Options \(f(g.buttonOptions?.value ?? 0))  Home \(f(g.buttonHome?.value ?? 0))",
        ]
    }
}
