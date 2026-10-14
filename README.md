<div align="center">
  <h1>macgip</h1>
  [![License](https://img.shields.io/badge/License-Apache--2.0-blue)](./LICENSE.md)
  [![Protected by Fencer](https://app.fencer.dev/trust/IG6oaYQ6BH5IlKljBuEZeqxpWid50W5M.svg)](https://app.fencer.dev/trust/IG6oaYQ6BH5IlKljBuEZeqxpWid50W5M/)
</div>
<br>
A macOS driver for wired gamepads that speak GIP, compatible with Xbox controllers. The first
target is the PowerA wired controller for Xbox Series X, USB ID `0x20d6:0x2007`, which macOS
does not support out of the box.

## Trademarks

Xbox, Xbox One and Xbox Series X are trademarks of the Microsoft group of companies. PowerA is a
trademark of its owner (ACCO Brands Corporation at the time of writing). This project is not
made, endorsed, sponsored or supported by Microsoft or PowerA. Trademarks appear here only to
state which products this software is compatible with. Device strings quoted from the hardware,
such as the USB product string, are reproduced verbatim as technical data.

## Licence

MIT. See `LICENSE`. Protocol facts were learned from the Linux `xpad` and `xone` drivers
(GPL-2.0) and from SDL (zlib). No code was copied from any of them.

Status 2026-10-11: the DriverKit extension works on the author's Mac with SIP on. It matches the pad, runs the GIP init, and publishes a HID gamepad that the Game Controller framework reports as an extended gamepad. Rumble, hot-plug, sleep and wake, and distribution signing are still open; see the issues. Read `notes/architecture.md` first.

## Layout

```
project.yml       XcodeGen spec for the Xcode project (mise run xcodeproj)
app/              the macgip host app: installs and removes the driver extension
driver/           the DriverKit extension: MacgipUSBDevice (configures the USB device) and
                  MacgipGamepad (GIP interface 0 as a HID gamepad)
gip/              protocol core in plain C with tests (mise run test)
notes/            findings: environment.md, device.md, protocol.md, disconnect.md, architecture.md, entitlements.md,
                  and handoff-2026-10-11.md (the original brief)
notes/captures/   raw descriptors, probe logs, test results
tools/            small programs used during investigation and verification
```

## Build and install the driver (development)

Needs Xcode, a paid Apple Developer account, and the setup in `notes/entitlements.md`: two App IDs, the development DriverKit capabilities, and a DriverKit development provisioning profile for the driver on this Mac.

```bash
mise run stage
```

That generates the project, builds the app and the driver with a fresh version stamp, copies the app to `/Applications`, and opens it. Click Install driver, approve the extension in System Settings, then plug the controller in. The guide LED lights when the driver has started. Before installing a new build, unplug the controller first so the old driver instance stops cleanly.

Uninstall: click Uninstall driver in the app, or remove the extension under System Settings > General > Login Items & Extensions > Driver Extensions.

## Tools

All tools build with the Xcode command line tools and Homebrew `libusb` and `sdl3`. None need `sudo`.

Dump USB descriptors (IOUSBLib, no exclusive open):

```bash
clang -o /tmp/usbdesc tools/usbdesc.c -framework IOKit -framework CoreFoundation && /tmp/usbdesc 0x20d6 0x2007
```

Probe the pad over libusb. Sends only the power-on packet, and the PowerA rumble begin and end packets if no input arrives:

```bash
clang -O1 -o /tmp/gip_probe tools/gip_probe.c $(PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig pkg-config --cflags --libs libusb-1.0) -lpthread && /tmp/gip_probe 60
```

The pad removes itself from the bus 30 to 90 s after the probe exits, because nothing keeps polling it. Replug it before the next run. See `notes/disconnect.md`.

Options: `--raw` (no decoding), `--no-init` (listen only), `--force-rumble` (send rumble even when input works), `--led` (send the guide LED packet after power-on). The number is the run time in seconds. Press every control during the run to verify the decode table in `notes/protocol.md`.

Check whether SDL3 sees the pad:

```bash
clang -o /tmp/sdl_test tools/sdl_test.c $(PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig pkg-config --cflags --libs sdl3) && SDL_HIDAPI_LIBUSB=1 SDL_HIDAPI_LIBUSB_WHITELIST=0 SDL_JOYSTICK_HIDAPI_XBOX_ONE=1 /tmp/sdl_test 10
```

The Homebrew `sdl3` bottle has no libusb support, so this reports 0 joysticks. Build SDL from source instead:

```bash
git clone --depth 1 https://github.com/libsdl-org/SDL.git /tmp/SDL && PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig cmake -S /tmp/SDL -B /tmp/SDL/build -DSDL_TESTS=OFF -DSDL_HIDAPI_LIBUSB=ON -DSDL_HIDAPI_LIBUSB_SHARED=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/SDL/build -j8
```

Then build the test against it and run it with the pad plugged in:

```bash
clang -o /tmp/sdl_test tools/sdl_test.c -I/tmp/SDL/include -L/tmp/SDL/build -lSDL3 -Wl,-rpath,/tmp/SDL/build && SDL_HIDAPI_LIBUSB=1 SDL_HIDAPI_LIBUSB_WHITELIST=0 SDL_JOYSTICK_HIDAPI_XBOX_ONE=1 /tmp/sdl_test 15
```

The test prints every device SDL's HID layer enumerates, then the joysticks SDL recognises, then button and axis events, and ends with a short rumble.

Virtual HID entitlement test (expected to fail with SIP on, see `notes/architecture.md`):

```bash
swiftc -O tools/vhid_test.swift -o /tmp/vhid && /tmp/vhid
```
