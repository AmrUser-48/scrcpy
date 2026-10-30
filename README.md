# scrcpy 3.3.4 — Debian 12 / Core 2 / keyboard gamepad

This branch is a focused modification of **upstream scrcpy 3.3.4**. It keeps the
scrcpy base version, Android server protocol, video/audio pipeline, SDL2
integration and existing keyboard/mouse behavior. The only application feature
added here is keyboard-driven gamepad input, enabled explicitly with
`--joystick` / `-j`.

The target is Debian 12 Bookworm on x86_64, including Intel Core 2 systems such
as the Dell Latitude D630. The Linux build uses Debian's shared SDL2 and FFmpeg
libraries. Those libraries are runtime dependencies, not copies bundled into
the executable.

## Feature overview

Start the modified controller mode with:

```sh
./scrcpy --joystick
# equivalent
./scrcpy -j
```

The option creates a second, software-originated gamepad in scrcpy's existing
Android HID gamepad path. Android identifies the HID device using the existing
Xbox 360-compatible descriptor and VID/PID (`045e:028e`, Microsoft X-Box 360
Pad). Input reports are sent through scrcpy's UHID transport.

This is **not** a Linux virtual input device:

- No host `uinput` device is created.
- No Python process or `python-uinput` dependency is required.
- No SDL virtual joystick is registered on the host.
- The keyboard mapping is translated directly into scrcpy gamepad axis and
  button events, then encoded by the existing HID gamepad implementation.
- The virtual gamepad exists on the Android side, alongside any physical
  controller already forwarded by scrcpy.

`--joystick` automatically selects `--gamepad=uhid`. It requires normal device
control and a scrcpy window; it is not supported with `--no-control`,
`--no-window` or OTG mode. The option is opt-in; without it, scrcpy's 3.3.4
input behavior is unchanged.

## Keyboard mapping

The default mapping follows the supplied keyboard-to-`uinput` prototype.

| Host key | Android gamepad input | Details |
| --- | --- | --- |
| `W` | Left stick up | `Y = -32768` |
| `A` | Left stick left | `X = -32768` |
| `S` | Left stick down | `Y = 32767` |
| `D` | Left stick right | `X = 32767` |
| `P` | Right stick right | Full positive X |
| `O` | Right stick left | Full negative X |
| `0` | Right stick right | Half positive X |
| `9` | Right stick left | Half negative X |
| `E` | A | South face button |
| `B` | B | East face button |
| `R` | X | West face button |
| `C` | Y | North face button |
| `Q` | Left shoulder | LB |
| `Tab`, `G` | Right shoulder | RB; either key holds it |
| `F`, Left Ctrl | Left trigger | LT; either key holds it |
| Left Alt | Right trigger | RT |
| `L` | Left stick click | L3 |
| `T` | Right stick click | R3 |
| `1` | D-pad up | Digital hat direction |
| `4` | Auto-X toggle | Enables/disables timed X presses |

Axes return to their centered value when their mapped keys are released. As in
the prototype, when opposite keys are held, the later check takes precedence:
D over A, W over S, 0 over P, 9 over 0, and O over P. The right stick Y axis is
currently held at center; the prototype's 5/6 mappings were commented out.

Mapped keys continue through scrcpy's normal keyboard-input path as well. The
gamepad mapping is additive; it does not turn the host keyboard into an
exclusive controller or remap ordinary text input.

### Automatic X button

Press `4` once to enable automatic X-button presses. The implementation sends
an initial short X press/release and then repeats it every **1.3 seconds**.
Press `4` again to disable it. The `R` key can still hold X manually; the
automatic pulse does not release a manually held X button.

The SDL timer callback only posts a user event. Gamepad reports are generated
on scrcpy's UI/event thread, not from the SDL timer thread. A single repeating
timer is used while the feature is enabled; disabling the feature removes it.
This keeps timer callbacks away from the controller/HID state and avoids
repeated timer destruction/recreation.

## Application changes

All application changes are isolated behind the new `--joystick` option.

### New keyboard gamepad module

- `app/src/keyboard_gamepad.h`
  - Keyboard gamepad state: held scancodes, button/axis state, auto-X state,
    SDL timer ID, custom event type and initialization state.
  - Init, destroy, key-event and timer-event interfaces.
- `app/src/keyboard_gamepad.c`
  - Maps SDL scancodes to the requested Xbox-style controls.
  - Tracks key-down/key-up transitions, ignores SDL key-repeat for button state
    and combines duplicate key bindings.
  - Recomputes the affected controller axes/buttons and emits only state
    transitions.
  - Implements the 1.3-second automatic X pulse.
  - Runs the timer callback separately from HID report generation; timer events
    are handled on the UI/event thread.

### Input manager and screen integration

- `app/src/input_manager.h`, `app/src/input_manager.c`
  - Add the keyboard gamepad state to the input manager.
  - Initialize it only when `options->joystick` is enabled.
  - Forward SDL keyboard and custom timer events to the new module.
  - Destroy the virtual gamepad and release held controls during shutdown.
- `app/src/screen.h`, `app/src/screen.c`
  - Pass the opt-in keyboard-gamepad setting into input-manager initialization.
  - Handle initialization failure through the screen cleanup path.
- `app/src/options.h`, `app/src/options.c`
  - Add the default-disabled `joystick` option.
- `app/src/cli.c`
  - Add `-j` and `--joystick`.
  - Validate control/window/OTG constraints.
  - Automatically select the UHID gamepad transport.
- `app/src/scrcpy.c`
  - Connect the option to screen initialization and release the keyboard gamepad
    before stopping the controller/HID transport.
- `app/src/input_events.h`
  - Reserve a dedicated gamepad ID for the keyboard-created gamepad.
- `app/src/uhid/gamepad_uhid.c`
  - Give the keyboard-created device a safe name without attempting to look it
    up as an SDL physical controller.
  - Reuse the normal Xbox 360 HID descriptor and report encoder.

### Documentation and tests

- `doc/gamepad.md`: documents the keyboard map and auto-X behavior.
- `app/scrcpy.1`: documents `-j` / `--joystick`.
- Bash and Zsh completion files: include the new option.
- `app/tests/test_cli.c`: verifies both `--joystick` and `-j` parsing and the
  automatic UHID selection.
- `app/meson.build`: compiles the new keyboard gamepad source file.

The implementation does not replace or patch the scrcpy Android server's HID
protocol. It reuses the existing 3.3.4 UHID gamepad transport and descriptor.

## Build target

The supported native target is:

- Debian 12 Bookworm, x86_64.
- Intel Core 2 compatible instruction set.
- SDL 2.26 from Debian 12.
- FFmpeg 5.1 shared libraries from Debian 12.
- X11/OpenGL.
- Matching `scrcpy-server` built from this branch's scrcpy 3.3.4 source.

The client is dynamically linked against the system libraries. The release
archive contains the client, matching Android server and license/readme only;
it does not bundle SDL2, FFmpeg, libusb or adb.

### Compiler and linker flags

The safe-aggressive application profile is:

```sh
CFLAGS="-O3 -march=core2 -mtune=core2 -pipe -fomit-frame-pointer -ftree-vectorize"
CXXFLAGS="${CFLAGS}"
LDFLAGS="-Wl,-O1 -Wl,--as-needed"
```

Debian hardening flags remain enabled in the build environment:

- `-D_FORTIFY_SOURCE=2`
- `-fstack-protector-strong`
- `-Wformat`
- `-Werror=format-security`
- linker `-z relro` and `-z now`
- Debian GCC's PIE defaults

The build uses Meson's release mode, symbol stripping and LTO. It does not use
`-march=native`, AVX, AVX2 or a newer CPU target. `-march=core2` is the
instruction-set ceiling; `-ftree-vectorize` may vectorize only for that target.

**FFmpeg clarification:** this Debian-native profile links to Debian 12's
prebuilt FFmpeg shared libraries. The flags above compile this branch's scrcpy
client; they do not rebuild or retune Debian's FFmpeg packages. To build FFmpeg
from source with the same profile, pass the same CFLAGS/CXXFLAGS to FFmpeg's
configure/make build and retain the same linker hardening flags.

### Debian 12 build dependencies

The CI container installs the development libraries from Debian 12:

```sh
sudo apt install \
  gcc g++ make pkg-config meson ninja-build binutils file \
  libsdl2-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswresample-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxfixes-dev libxi-dev libxss-dev libxtst-dev \
  libxinerama-dev libxkbcommon-dev \
  libgl1-mesa-dev libegl1-mesa-dev libgles2-mesa-dev \
  libasound2-dev libudev-dev libusb-1.0-0-dev
```

The Android server build additionally uses OpenJDK 17, Android command-line
tools, platform 36 and build-tools 36.0.0.

### Build commands

Build the matching Android server:

```sh
export ANDROID_HOME=/opt/android-sdk
export ANDROID_SDK_ROOT="${ANDROID_HOME}"
export PATH="${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools:${PATH}"

ANDROID_PLATFORM=36 \
ANDROID_BUILD_TOOLS=36.0.0 \
BUILD_DIR="${PWD}/server/build_manual" \
  ./server/build_without_gradle.sh
```

Build the client against Debian's shared libraries:

```sh
export CFLAGS="-O3 -march=core2 -mtune=core2 -pipe -fomit-frame-pointer -ftree-vectorize -D_FORTIFY_SOURCE=2 -fstack-protector-strong -Wformat -Werror=format-security"
export CXXFLAGS="${CFLAGS}"
export LDFLAGS="-Wl,-O1 -Wl,--as-needed -Wl,-z,relro -Wl,-z,now"

meson setup build \
  --buildtype=release \
  --strip \
  -Db_lto=true \
  -Dstatic=false \
  -Dportable=true \
  -Dv4l2=false \
  -Dprebuilt_server="${PWD}/server/build_manual/scrcpy-server"

ninja -C build
```

### Build validation

CI verifies:

- scrcpy reports version `3.3.4`.
- `--help` exposes `--joystick`.
- the executable is stripped and smaller than 512 KiB (expected size is in the
  build log; the previous system-library build was approximately 180 KiB).
- `ldd` resolves SDL2 and Debian 12 FFmpeg SONAMEs:
  `libSDL2-2.0.so`, `libavcodec.so.59`, `libavformat.so.59`,
  `libavutil.so.57` and `libswresample.so.4`.
- the client has no unresolved shared-library dependencies.
- disassembly contains no AVX/AVX2-class instructions unavailable on Core 2.
- the packaged client and Android server both exist and the client can locate
  the adjacent server.

The release is a Debian 12 x86_64 tarball with `scrcpy`,
`scrcpy-server`, `LICENSE` and this README. The workflow publishes it as a
GitHub Release only after the build and validation steps succeed.

## Runtime

Install Debian 12 runtime dependencies:

```sh
sudo apt install adb libsdl2-2.0-0 libavcodec59 libavformat59 libavutil57 \
  libswresample4 libusb-1.0-0
```

Extract the release archive and keep `scrcpy` and `scrcpy-server` in the
same directory. Run:

```sh
./scrcpy
./scrcpy --joystick
```

For a Core 2 Duo / Intel GMA X3100 class system, a lower capture resolution and
H.264 can reduce CPU/GPU load:

```sh
./scrcpy --joystick --max-size=1024 --video-codec=h264 --no-audio
```

## Version boundary

This branch remains scrcpy **3.3.4**. It does not rebase to a newer upstream
version or change the server/client protocol. The joystick feature is an
opt-in addition to the existing 3.3.4 application.

## License

The upstream project and this branch are distributed under the Apache License
2.0. See [LICENSE](LICENSE).
