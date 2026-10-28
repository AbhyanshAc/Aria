# Aria Arduino Library

This library is proprietary and distributed only through authorized ARIA
project channels and the AbhyanshAc/Aria repository. Do not copy, modify, or redistribute it without written permission from the project owner.

This lightweight, hardware-agnostic library lets Arduino sketches communicate
with ARIA applications over USB Serial. It does not detect board families; if
the board's serial implementation works with the Arduino IDE, the library can
use it.

The library targets Arduino cores that provide the standard `Stream` and
`String` APIs, and `library.properties` declares `architectures=*`. The only
AVR-specific code is the base64 lookup table's `PROGMEM` access; other cores
use their normal constant-data storage. Boards with a compatible USB serial
port can use `Serial`; boards that expose USB through another `Stream` object
can pass that object to `ARIA.begin(...)` instead. A board that cannot present
a serial `Stream` to the companion app needs a transport adapter and is not
usable over this USB-serial library by itself.

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

### Ask the app's AI from firmware

Questions are asynchronous. Stop or hold the device safely while waiting; the
response callback runs from `ARIA.loop()` and receives a temporary string.

```cpp
char guidance[161];

void onGuidance(const char *requestId, const char *response) {
  // Match requestId if the sketch can have more than one question in flight.
  strncpy(guidance, response, sizeof(guidance) - 1);
  guidance[sizeof(guidance) - 1] = '\0';
  // Validate the guidance against device-side limits before acting.
}

void setup() {
  Serial.begin(115200);
  ARIA.begin(Serial, "line-robot");
  ARIA.onAIResponse(onGuidance);
}

void loop() {
  ARIA.loop();
  // At an ambiguous junction, stop and ask once:
  // ARIA.askAI("Two unmarked branches. Suggest a safe test sequence.", "route");
}
```

## Arduino Uno memory profile

The default capacities are conservative for an Uno's 2 KB SRAM: three
actions, five variables, three events, a 192-byte input frame, 128-character
raw64 chunks, and 256 characters of total raw64 payload. These defaults also
work on larger Arduino boards; increase any capacity before including
`Aria.h` when the sketch and board have more memory. For example:

```cpp
#define ARIA_MAX_ACTIONS 8
#define ARIA_MAX_VARS 12
#define ARIA_MAX_EVENTS 8
#define ARIA_LINE_BUFFER_SIZE 512
#include "Aria.h"
```

The larger frame buffer is useful for longer AI questions and raw64 chunks;
the app limits AI replies to 160 UTF-8 bytes for compatibility with the default
Uno-sized frame. The protocol itself is not tied to AVR, but available SRAM,
flash, serial transport, and Arduino-core API support vary by board.
