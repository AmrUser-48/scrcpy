# Gamepad

Several gamepad input modes are available:

 - `--gamepad=disabled` (default)
 - `--gamepad=uhid` (or `-G`): simulates physical HID gamepads using the UHID
   kernel module on the device
 - `--gamepad=aoa`: simulates physical HID gamepads using the AOAv2 protocol


## Physical gamepad simulation

Two modes allow to simulate physical HID gamepads on the device, one for each
physical gamepad plugged into the computer.


### UHID

This mode simulates physical HID gamepads using the [UHID] kernel module on the
device.

[UHID]: https://kernel.org/doc/Documentation/hid/uhid.txt

To enable UHID gamepads, use:

```bash
scrcpy --gamepad=uhid
scrcpy -G  # short version
```

Note: UHID may not work on old Android versions due to permission errors.


### AOA

This mode simulates physical HID gamepads using the [AOAv2] protocol.

[AOAv2]: https://source.android.com/devices/accessories/aoa2#hid-support

To enable AOA gamepads, use:

```bash
scrcpy --gamepad=aoa
```

Contrary to the other mode, it works at the USB level directly (so it only works
over USB).

It does not use the scrcpy server, and does not require `adb` (USB debugging).
Therefore, it is possible to control the device (but not mirror) even with USB
debugging disabled (see [OTG](otg.md)).

Note: For some reason, in this mode, Android detects multiple physical gamepads
as a single misbehaving one. Use UHID if you need multiple gamepads.

Note: On Windows, it may only work in [OTG mode](otg.md), not while mirroring
(it is not possible to open a USB device if it is already open by another
process like the _adb daemon_).


## Keyboard-to-gamepad mapping

Use `scrcpy --joystick` (or `scrcpy -j`) to expose a virtual Xbox 360-style
HID gamepad to Android and drive it from the computer keyboard. This uses
scrcpy's existing UHID transport; no host-side `uinput` device is created.

| Keyboard | Gamepad input |
| --- | --- |
| `W`, `A`, `S`, `D` | Left stick |
| `P`, `O` | Right stick right / left |
| `0`, `9` | Right stick right / left at half range |
| `E`, `B`, `R`, `C` | A, B, X, Y |
| `Q`, `Tab` | Left / right shoulder |
| `G` | Right shoulder (alternate key) |
| `L`, `T` | Left / right stick click |
| `1` | D-pad up |
| `F`, Left Ctrl | Left trigger |
| Left Alt | Right trigger |
| `4` | Toggle automatic X-button press every 1.3 seconds |

Opposing stick directions use the same priority as the original mapping
(D over A, W over S; 9 over 0, O over P). Duplicate bindings are combined,
so releasing one key does not cancel the other.
