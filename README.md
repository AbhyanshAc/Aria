# Aria Arduino Library

This library is proprietary and distributed only through authorized ARIA
project channels and the AbhyanshAc/Aria repository. Do not copy, modify, or redistribute it without written
permission from the project owner.

This lightweight, hardware-agnostic library lets Arduino sketches communicate
with ARIA applications over USB Serial. It does not detect board families; if
the board's serial implementation works with the Arduino IDE, the library can
use it.

## Files

- `src/Aria.h`, `src/Aria.cpp` — core library
- `examples/simple_robot` — minimal action and variable registration example
  (the folder name is retained for existing users)

## Example

The `examples/fake_room_temperature` folder contains a sketch and matching
`aria.json` profile. It simulates a slowly varying room temperature around
22.5 °C and synchronizes the value every three seconds; it does not read a
physical sensor.

The profile's `device.baudRate` must match the sketch's `Serial.begin(...)`.
To try it, install the library in the Arduino IDE, open
`fake_room_temperature.ino`, select Arduino Uno and its serial port, and
upload. In ARIA's metadata editor, import the adjacent `aria.json` profile and
connect the Uno over USB. Close Arduino IDE's Serial Monitor before connecting
so ARIA can use the serial port. The AI receives the runtime `temperatureC`
reading, and the profile identifies it as simulated.

## Usage

Copy `src/` into a standard Arduino library layout, or include `Aria.h` and
`Aria.cpp` directly in your sketch folder. Keep device metadata in an
`aria.json` or `aria.yaml` profile rather than firmware. The library supports
`bool`, `int`, `float`, `string`, `json`, and `raw64` variables; JSON and raw64
values use Arduino `String` storage.

The application-to-firmware framing, message types, discovery, typed values,
and raw64 transfer format are described in the
[ARIA v2 protocol](../../../docs/ARIA_protocol.md). See that document for the
library API and protocol responsibilities.

## Arduino Uno memory profile

The default limits are sized for the Uno's 2 KB SRAM: three actions, five
variables, three events, 320-byte input frames, 128-character raw64 chunks,
and 256 characters of total raw64 payload. Larger boards can override `ARIA_MAX_ACTIONS`, `ARIA_MAX_VARS`,
`ARIA_MAX_EVENTS`, `ARIA_LINE_BUFFER_SIZE`, `ARIA_MAX_RAW64_LENGTH`, and
`ARIA_RAW64_CHUNK_SIZE` before including `Aria.h`.
