# The pad drops off the bus after a probe run

Observed 2026-10-11. The owner reported that the pad "keeps crashing" and must be unplugged and replugged.

## Timeline from the kernel log

Source: `log show --predicate 'process == "kernel" AND eventMessage CONTAINS "Xbox Series X Wired Controller Red"'`.

| Time | Event | Note |
|---|---|---|
| 14:13:27 | enumerated | plugged in, unconfigured, no driver |
| 14:19:38 | `gip_probe` set configuration 1 | run 1 (12 s) |
| 14:20:22 | `terminateDevice: hardware connection lost` | 32 s after run 1 ended |
| 14:48:22 | enumerated | owner replugged |
| 14:48:38 | `gip_probe` set configuration 1 | run 2 (about 17 s) |
| 14:50:22 | `hardware connection lost` | about 87 s after run 2 ended |
| 14:57:12 | enumerated | owner replugged |
| 14:57:43 | `gip_probe` set configuration 1 | run 3 (26 s) |
| 14:58:48 | `hardware connection lost` | 36 s after run 3 ended |
| 15:02:xx | enumerated | owner replugged |
| 15:03:2x | `gip_probe --led` run 4 (10 s), then `sdl_test` 15 s | SDL exited at 15:03:53 |
| 15:04:20 | `hardware connection lost` | 27 s after SDL exited, nobody touched the cable |

Pattern: an untouched, unconfigured pad stayed enumerated for 7 minutes. A pad that was started with the power-on packet and then abandoned (interface released, process exited, nobody polling the IN endpoint) disconnected itself 30 to 90 s later every time. No crash reports exist for `gip_probe` in `~/Library/Logs/DiagnosticReports`.

`hardware connection lost` means the hub saw the device remove itself from the bus. Nothing on the Mac side terminated it.

## Interpretation

The pad is not crashing. Once started, a GIP pad expects a live host that keeps an interrupt IN transfer pending. When the host stops polling, the pad's firmware gives up and resets itself off the bus. First-party controllers for Xbox behave the same way when a console goes to sleep. Apple's driver, SDL, and xone keep the endpoint polled for as long as the pad is plugged in, so this never happens with a real driver. The probe is the only thing that starts the pad and then walks away.

## What this means for the project

- The driver must keep an IN transfer pending at all times from `Start` until the device is removed. This is normal for a DriverKit HID device.
- Short test tools must expect the pad to vanish within about a minute after they exit. Replug it, or keep the tool running.
- Whether a clean power-off packet before exit (`05 20 <seq> 01 04`, GIP power mode "off" in xone) avoids the self-reset is untested. Sending it is outside the packets the handoff allows, so it needs the owner's approval first.

## Confirmed

The 15:04:20 disconnect happened while the owner was at the keyboard reporting the LED and rumble results, with no hand on the cable. The disconnect also follows an SDL session, not only the probe, so it is not specific to how the probe closes the device.

## Hold test (verified)

Run 5 (`captures/2026-10-11-probe-run5-hold-300s.txt`): `gip_probe 300 --led` from 15:10:12 to 15:15:12. The pad stayed enumerated for the whole run, with zero transfer errors, and was still on the bus when the probe exited. While polled, the pad is stable. Only the abandoned state triggers the self-reset.

## Still to verify

- Whether a power-off packet before exit avoids the self-reset (needs owner approval to send).
