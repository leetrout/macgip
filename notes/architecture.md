# Architecture: evidence and recommendation

Written 2026-10-11 after Phase 1 and Phase 2. Updated the same day with the owner's answers.

## Owner's answers (2026-10-11)

1. Scope: everything. System Settings, browsers, Game Controller framework games, SDL, Steam.
2. Paid Apple Developer account: yes.
3. SIP and boot arguments: the owner asked for an explanation. See "What the security settings mean" below.
4. Shared project, not personal only. So the end result must install on other Macs with SIP on. That rules out any design that needs SIP off at install time.
5. `cmake` installed by the owner. SDL source build in progress for Option C.
6. The owner ran the probe. Decode table verified. See `protocol.md`.

## What the security settings mean

- SIP (System Integrity Protection) stops anything, even root, from changing system files and from loading unsigned or wrongly entitled system code. Turning it off is done from Recovery. With SIP off, any process running as root can modify the OS. It is a development-only state.
- `amfi_get_out_of_my_way=1` is a boot argument that turns off AMFI (Apple Mobile File Integrity). AMFI is the part that enforces code signatures and restricted entitlements. With it off, any binary can claim any entitlement. This is a larger reduction than SIP off alone. thrustmacos needs it because its dext is self-signed and claims restricted DriverKit entitlements.
- `systemextensionsctl developer on` only lets a dext load from a build directory instead of `/Applications`. It is harmless by itself and needs SIP off to be set.

None of these are needed when Apple has granted the entitlements. They are the workaround for not having them. Because the project is to be shared and the owner has a paid account, the plan is to request the entitlements and keep SIP on.

## What we know

- The pad is a standard GIP device (`FF/47/D0`). See `device.md`.
- Apple's `XboxGamepad.dext` matches only Microsoft IDs. It never loads for `0x20d6:0x2007`. See `device.md`.
- A userspace libusb client can claim interface 0 without `sudo`, and the power-on packet alone starts input. See `protocol.md`.
- The owner's Mac runs macOS 26.5.2 on Apple Silicon with SIP enabled. See `environment.md`.

## Option A: userspace daemon plus virtual HID gamepad

Tested 2026-10-11. Result: not possible on this machine without an Apple-granted entitlement. Evidence in `captures/2026-10-11-vhid-entitlement-test.txt`.

- `IOHIDUserDeviceCreateWithProperties` and CoreHID `HIDVirtualDevice` both return nil for a plain local build.
- Adding `com.apple.developer.hid.virtual.device` to an ad-hoc signed binary makes AMFI kill the process at exec: "adhoc signed but contains restricted entitlements". SIP is on, so this cannot be bypassed locally.
- Apple's IOHIDFamily source (`IOHIDResourceUserClient.cpp`, `initWithTask`) accepts only two entitlements: `com.apple.hid.manager.user-access-device` (Apple internal) or `com.apple.developer.hid.virtual.device`. There is no root or admin fallback. Running the daemon as root does not help.
- Apple DTS, in developer forum thread 840311, says CoreHID is the intended way to build a virtual HID device and that the DriverKit virtual-device entitlement is not a real entitlement. The CoreHID entitlement must be requested from Apple through Capability Requests. That needs a paid Apple Developer account, and Apple decides whether to grant it.

So Option A only works if the owner has, or wants, a paid developer account and Apple grants the entitlement. If granted, Option A keeps SIP on and has the fastest build-run loop.

## Option B: DriverKit extension

Not tested yet. Facts from Apple docs and thrustmacos:

- A dext built on USBDriverKit can match `idVendor=0x20d6, idProduct=0x2007` on `IOUSBHostInterface` and subclass `IOUserHIDDevice`. That is exactly how Apple's own `XboxGamepad.dext` is structured (`IOClass=AppleUserHIDDevice` on the `FF/47/D0` interface). No "virtual" device is needed. The dext is the HID device for that USB interface.
- Entitlements: `com.apple.developer.driverkit`, `com.apple.developer.driverkit.transport.usb`, `com.apple.developer.driverkit.family.hid.device`. All are restricted.
- For a self-signed build, thrustmacos requires SIP disabled and the boot argument `amfi_get_out_of_my_way=1`, plus `systemextensionsctl developer on`. These are security setting changes and are the owner's call. The boot argument is a larger reduction in security than SIP off alone. The next session must verify whether SIP off plus developer mode is enough on macOS 26, without the AMFI boot argument.
- For a build other Macs can install, the owner needs a paid developer account and Apple must grant the DriverKit entitlements.

## Option C: app-specific, no system changes

SDL3 built with libusb can drive this pad through its own HIDAPI driver for Xbox One controllers:

- SDL commit `18fc4d931a942e36f372218902b6319d113d0edd` (2026-06-10) makes the libusb backend enumerate `FF/47/D0` interfaces on macOS and lets the Xbox One driver accept them. The MFi early return now applies only to IOKit HID paths (`DevSrvsID...`), not libusb devices.
- The newer GIP driver (`SDL_hidapi_gip.c`) still returns false for wired pads on macOS when MFi support is compiled in, so the legacy Xbox One driver is the one that will claim the pad. Use `SDL_JOYSTICK_HIDAPI_XBOX_ONE=1`, not `SDL_JOYSTICK_HIDAPI_GIP=1`.
- The Homebrew `sdl3` bottle (3.4.12 installed here) is built without libusb. `tools/sdl_test.c` against it sees 0 joysticks, with or without the libusb hints.
- Source build done 2026-10-11 (SDL main at `ce3af25d`, 2026-10-09, reports itself as 3.5.0) with `-DSDL_HIDAPI_LIBUSB=ON`. CMake found Homebrew libusb 1.0.30. The libusb backend is active at run time: `SDL_hid_enumerate` returns libusb-style paths such as `1-1.3.4:1.3`. See `captures/2026-10-11-sdl-run-d-verbose.txt`.
- The source build confirms the code path on paper: `src/hidapi/libusb/hid.c` `is_xboxone()` accepts interface 0 with `FF/47/D0` for vendor `0x20d6`, and `SDL_hidapijoystick.c` types the same interface as `SDL_GAMEPAD_TYPE_XBOXONE` for vendor `0x20d6` even though product `0x2007` is not in `controller_list.h`.
- The first run against the source build (`captures/2026-10-11-sdl-run-e-libusb-debug.txt`) was inconclusive because the pad had already dropped off the bus (see `disconnect.md`).
- Rerun with the pad present (`captures/2026-10-11-sdl-run-f-pad-present.txt`): SDL enumerates the pad through libusb (`path = 1-1.2:1.0`, interface `255/71/208`), assigns `SDL_JOYSTICK_HIDAPI_XBOX_ONE`, reports 1 joystick, type Xbox One, `SDL_IsGamepad` true, opens it, and `SDL_RumbleGamepad` runs the motors (owner felt it). Option C works with these environment variables: `SDL_HIDAPI_LIBUSB=1 SDL_HIDAPI_LIBUSB_WHITELIST=0 SDL_JOYSTICK_HIDAPI_XBOX_ONE=1`. The whitelist variable may be unnecessary because the macOS Xbox path bypasses the whitelist; untested without it.
- The pad had already been started by `gip_probe` in that run, so SDL's own init sequence from a fresh plug-in is untested.

This covers SDL3 games and anything built on SDL3. It does not cover System Settings, browsers, or Game Controller framework games.

## Recommendation (updated for "everything" and "share")

Request the entitlements from Apple now, because the wait is the long pole:

- For Option B (dext): `com.apple.developer.driverkit`, `com.apple.developer.driverkit.transport.usb` (scoped to vendor `0x20d6`), `com.apple.developer.driverkit.family.hid.device`. These are the standard DriverKit entitlements Apple grants to hardware driver developers. A granted dext installs on any Mac with SIP on, through a host app and one approval in System Settings. It is the shape Apple's own `XboxGamepad.dext` uses.
- For Option A (daemon plus CoreHID virtual device): `com.apple.developer.hid.virtual.device`. Newer, and Apple's willingness to grant it is less certain.

Recommend Option B as the product. It is the same architecture as Apple's own driver, needs no daemon, no libusb, and no virtual device, and it handles hot-plug and sleep through DriverKit's normal lifecycle. Request the Option A entitlement at the same time as a fallback.

While waiting for Apple:

1. Finish Option C (SDL with libusb) so the owner has something usable right away for SDL games.
2. Write the protocol core (`gip/`) with unit tests against `captures/`. It is shared by every option.
3. Draft the dext and its host app so they are ready to sign the day the entitlements arrive.

The earlier draft recommendation follows for reference.

1. If the owner only needs SDL-based games: install `cmake`, build SDL from source with libusb, and confirm with `tools/sdl_test.c`. Stop there and document the environment variables.
2. If the owner wants system-wide support and has a paid Apple Developer account: request the CoreHID entitlement first. Build Option A while waiting, since the protocol core is shared.
3. If the owner wants system-wide support with no developer account: Option B, which needs SIP off and developer mode on this Mac or on a second Mac. The dext matches the PowerA IDs directly and needs no virtual device.

In all cases the protocol core (`gip/`) is the same: init sequence, input parsing, rumble encoding, unit-tested against `captures/`.

## Still open for the owner

1. Licence for the shared project. Nothing from xpad or xone (GPL-2.0) has been copied. Packet bytes are facts, not code. SDL is zlib if we copy from it later.
2. Submit the entitlement requests in the Apple Developer portal (Certificates, Identifiers & Profiles > Capability Requests). The owner must do this; it is tied to the account.
3. Initialise the git repository.
