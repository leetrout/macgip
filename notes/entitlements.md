# Requesting the entitlements from Apple

Written 2026-10-11 from Apple's documentation. Two separate requests cover the two options in
`architecture.md`. Submit both. The Account Holder of the developer account must do this.

## Request 1: DriverKit entitlements (Option B, the dext)

Source: "Requesting Entitlements for DriverKit Development" (developer.apple.com/documentation/driverkit) and developer.apple.com/system-extensions.

Have ready before you start:

- Transport: USB.
- Vendor ID: `0x20d6` (PowerA). Apple asks for "your company's hardware vendor ID". We do not own this ID. State that the driver supports a third-party controller that macOS does not support, and give the vendor and product IDs `0x20d6:0x2007`.
- Device description: wired gamepad for Xbox Series X made by PowerA, GIP protocol, vendor-specific USB interface class `0xFF`, subclass `0x47`, protocol `0xD0`.
- App description: a small macOS app that installs and activates the driver extension and shows controller status.

Steps:

1. Sign in at https://developer.apple.com/system-extensions/ as the Account Holder.
2. In the "Request an Entitlement" section, click "Contact us". It opens https://developer.apple.com/contact/request/system-extension/ after sign-in.
3. Apply for DriverKit and list the complete set of entitlements for this one product. Apple ties the set to one group, and a provisioning profile can hold only one group, so the set must be complete:
   - `com.apple.developer.driverkit`
   - `com.apple.developer.driverkit.transport.usb`
   - `com.apple.developer.driverkit.family.hid.device`
4. Give the device and app descriptions above.
5. Submit. Apple attaches the granted group to the team. There is no published review time.

After approval:

1. Certificates, Identifiers & Profiles > Identifiers: register an App ID for the driver (the dext bundle ID) and one for the host app.
2. Profiles: create a DriverKit development provisioning profile for the driver App ID and select the DriverKit entitlements group Apple added to the team. Download it and set it as the driver's provisioning profile in Xcode.
3. In the dext's entitlements file, add the transport and family entitlements. The USB transport value is an array of dictionaries with `idVendor` `0x20d6` and `idProduct` `0x2007`.
4. On the host app target, enable the System Extension capability under Signing & Capabilities.

## Request 2: CoreHID virtual device entitlement (Option A, the fallback)

Source: developer.apple.com/help/account/capabilities/capability-requests.

Required role: Account Holder.

1. In Certificates, Identifiers & Profiles, click Identifiers in the sidebar.
2. Register an App ID for the daemon or host app if none exists, then click its name in the list.
3. Click the Capability Requests tab.
4. Find the virtual HID device capability (`com.apple.developer.hid.virtual.device`) and click Request.
5. Read the criteria Apple lists for the capability, then submit the request form. Describe the use: republish input from a USB gamepad that macOS has no driver for, as a virtual HID gamepad.

Check the status later: Identifiers > the App ID > Capability Requests tab > Status button.

After approval: Identifiers > the App ID > Capabilities tab > Enable the capability > Save. Then add it to the Xcode target.

## Notes

- Apple's documentation says that while a request is in review you can test on your own Mac by turning off System Integrity Protection. That is the owner's call and is not needed to keep working on the protocol core or on SDL support.
- Apple staff said in forum thread 840311 that `com.apple.developer.driverkit.family.hid.virtual.device` is not a real entitlement. Do not request it.
