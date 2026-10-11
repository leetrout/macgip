// Test: can a normal user process create a virtual HID gamepad with SIP on?
// Tries IOHIDUserDevice (IOKit) first, then CoreHID's HIDVirtualDevice.
import Foundation
import IOKit
import IOKit.hid
#if canImport(CoreHID)
import CoreHID
#endif

// Minimal generic gamepad: 16 buttons, 4 axes (X, Y, Rx, Ry), 2 triggers (Z, Rz). Our own descriptor, no borrowed IDs.
let desc: [UInt8] = [
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
      0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81, 0x02,
      0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x33, 0x09, 0x34, 0x16, 0x00, 0x80, 0x26, 0xFF, 0x7F, 0x75, 0x10, 0x95, 0x04, 0x81, 0x02,
      0x09, 0x32, 0x09, 0x35, 0x15, 0x00, 0x26, 0xFF, 0x03, 0x75, 0x10, 0x95, 0x02, 0x81, 0x02,
    0xC0
]

print("uid:", getuid())
let props: [String: Any] = [
    kIOHIDReportDescriptorKey as String: Data(desc),
    kIOHIDVendorIDKey as String: 0x1209,
    kIOHIDProductIDKey as String: 0x0001,
    kIOHIDProductKey as String: "vhid test gamepad",
    kIOHIDTransportKey as String: "Virtual",
]
if let dev = IOHIDUserDeviceCreateWithProperties(kCFAllocatorDefault, props as CFDictionary, 0) {
    print("IOHIDUserDevice: created", dev)
    print("sleeping 6 s so another process can run hidutil list")
    Thread.sleep(forTimeInterval: 6)
} else {
    print("IOHIDUserDevice: create FAILED (returned nil)")
}

#if canImport(CoreHID)
if #available(macOS 15.0, *) {
    let props = HIDVirtualDevice.Properties(descriptor: Data(desc), vendorID: 0x1209, productID: 0x0002, product: "corehid test gamepad")
    if let vdev = HIDVirtualDevice(properties: props) {
        print("CoreHID HIDVirtualDevice: created")
        let sem = DispatchSemaphore(value: 0)
        Task {
            do {
                try await vdev.dispatchInputReport(data: Data(repeating: 0, count: 14), timestamp: .now)
                print("CoreHID: dispatchInputReport ok")
            } catch { print("CoreHID: dispatchInputReport error:", error) }
            sem.signal()
        }
        _ = sem.wait(timeout: .now() + 5)
        print("sleeping 6 s so another process can run hidutil list")
        Thread.sleep(forTimeInterval: 6)
    } else {
        print("CoreHID HIDVirtualDevice: create FAILED (nil)")
    }
}
#endif
