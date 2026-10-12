import Foundation
import SystemExtensions

/// Installs and removes the driver extension and reports what happened.
@MainActor
final class ExtensionManager: NSObject, ObservableObject, OSSystemExtensionRequestDelegate {
    static let driverIdentifier = "com.leetrout.macgip.driver"

    @Published var status = "Not installed in this session."
    @Published var busy = false

    func install() {
        submit(OSSystemExtensionRequest.activationRequest(forExtensionWithIdentifier: Self.driverIdentifier, queue: .main))
    }

    func uninstall() {
        submit(OSSystemExtensionRequest.deactivationRequest(forExtensionWithIdentifier: Self.driverIdentifier, queue: .main))
    }

    private func submit(_ request: OSSystemExtensionRequest) {
        busy = true
        status = "Waiting for macOS..."
        request.delegate = self
        OSSystemExtensionManager.shared.submitRequest(request)
    }

    nonisolated func request(_ request: OSSystemExtensionRequest, actionForReplacingExtension existing: OSSystemExtensionProperties, withExtension ext: OSSystemExtensionProperties) -> OSSystemExtensionRequest.ReplacementAction {
        .replace
    }

    nonisolated func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
        Task { @MainActor in
            status = "Approve the extension in System Settings > General > Login Items & Extensions > Driver Extensions."
        }
    }

    nonisolated func request(_ request: OSSystemExtensionRequest, didFinishWithResult result: OSSystemExtensionRequest.Result) {
        Task { @MainActor in
            busy = false
            switch result {
            case .completed: status = "Done. Plug in the controller, then check System Settings > Game Controllers."
            case .willCompleteAfterReboot: status = "Done after the next restart."
            @unknown default: status = "Finished with an unknown result."
            }
        }
    }

    nonisolated func request(_ request: OSSystemExtensionRequest, didFailWithError error: Error) {
        Task { @MainActor in
            busy = false
            status = "Failed: \(error.localizedDescription)"
        }
    }
}
