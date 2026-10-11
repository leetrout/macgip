# Environment

Collected 2026-10-11.

| Item | Value |
|---|---|
| macOS | 26.5.2 (build 25F84) |
| Architecture | arm64 (Apple Silicon) |
| SIP | enabled |
| Xcode | 26.6 (build 17F113) at `/Applications/Xcode.app/Contents/Developer` |
| Homebrew | 7.0.6 |
| Python | 3.14.5 (rye shim); brew has python@3.12, 3.13, 3.14 |

## System extensions already present

```
--- com.apple.system_extension.driver_extension
	*	5JZGQTGU4W	cn.wch.CH34xVCPDriver (1.0/1)	[activated waiting for user]
--- com.apple.system_extension.cmio
*	*	2MMRE5MTB8	com.obsproject.obs-studio.mac-camera-extension (30.2.3)	[activated enabled]
```

Developer mode for system extensions is not enabled (`systemextensionsctl list` shows no developer mode line).

## Tools already installed (Homebrew)

- `libusb`, `libusb-compat`, `hidapi`
- `lsusb` (the Homebrew `lsusb` formula, version 1.0). It is a wrapper, not usbutils. `lsusb -v` prints nothing on this machine.

## Tools not installed

- `cmake`, `wireshark`/`tshark`, `usbutils`
- `pyusb` (no `usb` module in the default `python3`)

## Apple's own driver for Xbox controllers

`/System/Library/DriverExtensions/XboxGamepad.dext` exists. See `device.md` for its matching rules and whether it matched this pad.
