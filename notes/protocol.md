# Protocol findings: PowerA 0x20d6:0x2007

Source: `tools/gip_probe.c` runs on 2026-10-11. Raw logs are in `captures/`.

## Init sequence that works

1. Set configuration 1 and claim interface 0. No `sudo` is needed because no driver holds the interface.
2. The pad sends an announce packet (command `0x02`) every 500 ms until the host answers.
3. Send the power-on packet `05 20 <seq> 01 00` to endpoint `0x02`.
4. The pad answers within 5 ms with a status packet (`0x03`) and then an input report (`0x20`).

The PowerA rumble trick from xpad was not needed on this unit. The power-on packet alone starts input.

## Packets seen (run 1)

Announce, repeated 5 times before init, 32 bytes:

```
02 20 01 1c ab 9c 3c 12 c0 6d 00 00 d6 20 07 20 01 00 05 00 a7 00 4d 05 02 0e 01 00 01 00 01 00
```

Decoding, from the GIP write-up and xone:

| Offset | Bytes | Meaning |
|---|---|---|
| 0 | `02` | command: announce |
| 1 | `20` | flags (system, needs no ack) |
| 2 | `01` | sequence |
| 3 | `1c` | payload length 28 |
| 4..11 | `ab 9c 3c 12 c0 6d 00 00` | device address / serial, little endian |
| 12..13 | `d6 20` | vendor ID `0x20d6` |
| 14..15 | `07 20` | product ID `0x2007` |
| 16..31 | `01 00 05 00 a7 00 4d 05 02 0e 01 00 01 00 01 00` | firmware and hardware versions |

Status after power-on, 8 bytes:

```
03 20 01 04 80 00 00 00
```

Byte 4 `0x80`: bit 7 set is reported as "powered / full" in xone's status decoding. Verify against `xone/bus/protocol.c` before relying on it.

Input report, 36 bytes, sticks at rest:

```
20 00 01 20 00 00 00 00 00 00 40 07 b4 fe 46 04 4b fe 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

| Offset | Meaning |
|---|---|
| 0 | command `0x20` input |
| 1 | flags `0x00` |
| 2 | sequence |
| 3 | payload length `0x20` = 32. First-party controllers for Xbox send 14. This pad pads with 18 zero bytes. |
| 4 | buttons: bit2 menu, bit3 view, bit4 A, bit5 B, bit6 X, bit7 Y |
| 5 | bit0 up, bit1 down, bit2 left, bit3 right, bit4 LB, bit5 RB, bit6 L-stick click, bit7 R-stick click |
| 6..7 | left trigger, u16, 0..1023 |
| 8..9 | right trigger, u16, 0..1023 |
| 10..11 | left X, s16 (here 1856) |
| 12..13 | left Y, s16 (here -332) |
| 14..15 | right X, s16 (here 1094) |
| 16..17 | right Y, s16 (here -437) |

Verified on this unit by the owner on 2026-10-11 (`captures/2026-10-11-probe-run2-owner.txt`): A, B, X, Y, menu, view, all four d-pad directions, LB, RB, both triggers over 0..1023, and all four stick axes over -32768..32767. Stick clicks verified in run 3 (`captures/2026-10-11-probe-run3-sticks.txt`): left click sets byte 5 bit 6 (`0x40`), right click sets byte 5 bit 7 (`0x80`). Every input bit in the table is now verified on this unit.

The pad sends input reports on change only. With nobody touching it, one report arrived after init and then silence.

## Guide button (verified)

The guide button arrives as command `0x07`, 6 bytes:

```
07 20 02 02 01 5b   pressed
07 20 03 02 00 5b   released
```

Byte 4 is the state. Byte 5 (`0x5b`) is constant in both packets. The probe sent no ack and the pad kept reporting normally, so the ack packet that xpad sends is not required for input to continue. Whether the pad repeats the guide packet until acked was not checked (only one press and one release appear in the log, 220 ms apart, no repeats).

## Guide LED (verified)

The guide LED stays off until the host sends the LED command. Run 4 (`captures/2026-10-11-probe-run4-led.txt`) sent, 2 s after power-on:

```
0a 20 <seq> 03 00 01 14
```

Byte 5 is the mode (`0x01` = on), byte 6 the brightness (`0x14`). The owner confirmed the white LED lit. SDL and xone send the same packet in their init sequence.

## Status packet

Command `0x03`, 8 bytes, seen once at init and once more mid-run:

```
03 20 03 04 80 00 00 00
```

Byte 4 `0x80` was constant. Treat it as a battery or power status and ignore it for now.

During the 5 minute hold (run 5) the pad sent a status packet about every 20 s with no host request. Treat it as a heartbeat. The driver must tolerate it and should not ack it (flags byte `0x20`, no ack requested).

## Run 2 observations

- The pad was still configured (configuration 1) from run 1 and sent a status packet, not an announce, when claimed. After a fresh plug-in it sends announce packets instead. Handle both.
- Input report rate while a stick moves is about 4 to 8 ms between packets.
- The GIP sequence byte (offset 2) wraps at 255.

## Not yet verified

- The PowerA rumble packets from xpad (`09 00 ...`) on this unit. SDL's rumble call on 2026-10-11 made the motors run (owner confirmed), so SDL's rumble packet for Xbox One controllers works on this unit. The xpad PowerA packets themselves are untested.
- Behaviour after unplug and replug, and after sleep.

## HID report layout the driver publishes (verified 2026-10-12)

The driver turns each GIP input packet into HID input report 1, 15 bytes with the report ID. The layout follows what IOHIDFamily's `IOHIDEventDriver::checkGameControllerElement` needs to classify an extended game controller, plus the three Consumer keys that gamecontrollerd maps to the system buttons.

| Bytes | Usage | Game Controller framework element |
|---|---|---|
| 1..4 | Generic Desktop X, Y | Left thumbstick |
| 5..8 | Generic Desktop Z, Rz | Right thumbstick |
| 9..12 | Button 7, Button 8 (0..1023) | Left trigger, right trigger (analog) |
| 13 bits 0..3 | Generic Desktop DPad Up (0x90), Down (0x91), Right (0x92), Left (0x93) | Direction pad |
| 13 bits 4..7 | Button 1..4 | A, B, X, Y |
| 14 bits 0..1 | Button 5, 6 | L1, R1 |
| 14 bits 2..3 | Button 9, 10 | Left and right thumbstick buttons |
| 14 bit 4 | Consumer AC Exit (0x204) | Menu |
| 14 bit 5 | Consumer AC Properties (0x209) | Options |
| 14 bit 6 | Consumer AC Home (0x223) | Home. macOS handles it and opens the Games app. |

Output report 2 is 4 bytes: left trigger, right trigger, low and high motor strength, 0..100.

Probe results that shaped this (`captures/2026-10-12-*-probe.txt`):

- A hat switch and Simulation Accelerator/Brake do not count for classification. The device was a HID gamepad for IOHIDManager clients but invisible to `GCController`.
- Button 11 and 12 arrive as L4 and R4. Button 13..15 arrive as M1..M3.
- AC Home alone arrives as Menu. With AC Exit present, AC Exit is Menu and AC Home becomes Home.
- Record (0xB2) arrives as Share.
- Consumer Menu, AC Back, AC Search, Menu Pick, Menu Escape, AL Control Panel, AC Stop, AC Desktop Show All Windows, and Generic Desktop System Main Menu and System App Menu arrive as nothing.
- A report shorter than the descriptor declares is dropped silently. Keep report 1 at exactly 112 data bits.
- Axis signs (verified 2026-10-12 in the app's preview): the framework reports the HID Y axis as is, so the driver passes GIP Y through unchanged and up is positive. 0x92 is DPad Right and 0x93 is DPad Left in the HID usage table.
