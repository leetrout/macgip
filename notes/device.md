# Device: the PowerA wired controller for Xbox Series X (red)

Collected 2026-10-11 on macOS 26.5.2, Apple Silicon, SIP enabled.

## Identity

| Field | Value |
|---|---|
| Vendor ID | `0x20d6` (8406), string "PowerA" |
| Product ID | `0x2007` (8199) |
| Product string | "Xbox Series X Wired Controller Red" |
| Serial | `00006DC0123C9CAB` |
| bcdDevice | `0x0167` |
| bcdUSB | `0x0200` |
| Device class / subclass / protocol | `0xFF` / `0xFF` / `0xFF` |
| bMaxPacketSize0 | 64 |
| Speed | full speed, 12 Mbit/s |
| Configurations | 1 |
| Current configuration at plug-in | 0 (not configured; nothing claimed it) |

`UsbDeviceSignature` in the registry: `d620 0720 6701 <serial> ffffffff 47d0`.

## Configuration descriptor

Raw bytes are in `captures/2026-10-11-descriptors.txt`. Collected with `tools/usbdesc.c` (IOUSBLib, no root).

Configuration 1: 3 interfaces, bus powered, remote wake, 500 mA.

| Interface | Alt | Class/Sub/Proto | Endpoints |
|---|---|---|---|
| 0 | 0 | `FF/47/D0` | `0x02` OUT interrupt 64 B, interval 4; `0x82` IN interrupt 64 B, interval 4 |
| 1 | 0 | `FF/47/D0` | none |
| 1 | 1 | `FF/47/D0` | `0x03` OUT iso 228 B, interval 1; `0x83` IN iso 228 B, interval 1 (audio) |
| 2 | 0 | `FF/47/D0` | none |
| 2 | 1 | `FF/47/D0` | `0x01` OUT bulk 64 B; `0x81` IN bulk 64 B |

This is the standard GIP layout: interface 0 carries control and input, interface 1 is headset audio, interface 2 is bulk data.

## What macOS does with it

- `ioreg`: the `IOUSBHostDevice` node has no `IOUSBHostInterface` children and no `kUSBCurrentConfiguration`. No driver matched.
- The only clients on the device node are non-exclusive `AppleUSBHostDeviceUserClient` entries from Google Chrome, Spotify and Android File Transfer Agent. These are enumeration-only clients. They do not configure the device.
- `hidutil list` and `ioreg -c IOHIDDevice`: no entry for the pad.
- Game Controller framework (`GCController.controllers()` from a small Swift program): 0 controllers.
- So the pad does not appear in System Settings > Game Controllers. It cannot appear in a browser gamepad tester either, because nothing publishes it as HID.

## Why Apple's driver skips it

`/System/Library/DriverExtensions/XboxGamepad.dext` (bundle `com.apple.gamecontroller.driver.XboxGamepad`, version 13.5.1) matches by exact vendor and product ID only. Its personalities:

| Personality | idVendor | idProduct | Interface match |
|---|---|---|---|
| Xbox360Gamepad | `0x045e` | `0x028e` | `FF/5D/01` |
| XboxOneSGamepad | `0x045e` | `0x02ea` | `FF/47/D0` |
| XboxEliteV2Gamepad | `0x045e` | `0x0b00` | `FF/47/*` |
| XboxSeriesXGamepad | `0x045e` | `0x0b12` | `FF/47/*` |

Each also has a device-level `XboxUSBDevice` personality on `IOUSBHostDevice` with the same IDs. There is no class-only personality, so a third-party GIP pad at `0x20d6:0x2007` never matches. This is not an init-sequence problem on Apple's side. Apple's driver simply never loads for this ID.

## Decision

Handoff table row: "Interface is `FF/47/D0`, no driver attached". Go to Phase 2.

Because no driver holds the interface, a userspace libusb client can claim interface 0 without `sudo`.

## Second device: PDP Afterglow Wave Wired Controller for Xbox

Collected 2026-10-22 on the contributor's Mac (macOS on Apple Silicon, SIP enabled). Raw descriptor bytes are in `captures/2026-10-22-pdp-descriptors.txt`. The serial number is left out on purpose: the GIP device address in the announce packet is derived from it.

| Field | Value |
|---|---|
| Vendor ID | `0x0e6f` (3695), string "Performance Designed Products" |
| Product ID | `0x0338` (824) |
| Product string | "PDP Afterglow Wave Wired Controller for Xbox- Purple" |
| bcdDevice | `0x0300` |
| bcdUSB | `0x0200` |
| Device class / subclass / protocol | `0xFF` / `0x47` / `0xD0` |
| bMaxPacketSize0 | 64 |
| Speed | full speed, 12 Mbit/s |
| Configurations | 1 |

Configuration 1: 2 interfaces, bus powered, remote wake, 500 mA.

| Interface | Alt | Class/Sub/Proto | Endpoints |
|---|---|---|---|
| 0 | 0 | `FF/47/D0` | `0x81` IN interrupt 64 B, interval 4; `0x01` OUT interrupt 64 B, interval 4 |
| 1 | 0 | `FF/47/D0` | none |
| 1 | 1 | `FF/47/D0` | `0x02` OUT iso 214 B, interval 1; `0x83` IN iso 118 B, interval 1 (audio) |

Differences from the PowerA pad:

- The interrupt endpoints of interface 0 are `0x81` and `0x01`, not `0x82` and `0x02`. Read the endpoint addresses from the interface descriptor. Do not hard-code them.
- There is no interface 2 (bulk data).
- The device subclass and protocol are `0x47` and `0xD0`, not `0xFF` and `0xFF`. The device class is `0xFF` on both.

Apple's `XboxGamepad.dext` does not match this ID either. Its personalities name Microsoft IDs only (see above). No driver held interface 0, and a libusb client claimed it without `sudo`. A Chrome process had the device open (an `AppleUSBHostDeviceUserClient`) at the same time. The claim still worked.
