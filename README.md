# ARIA — Universal AI for Arduino-compatible USB devices

ARIA is proprietary, closed-source software. Repository access, firmware
library use, and redistribution are controlled by the project owner; no
implied license to copy, modify, or redistribute this code is granted.

ARIA connects a phone or desktop app to Arduino-compatible microcontrollers
over USB serial. Firmware exposes compact actions, variables, and events;
separate `aria.json` or `aria.yaml` metadata describes device capabilities,
units, ranges, and AI guidance. The app supports OpenAI, Google Gemini,
Anthropic Claude, Groq, OpenRouter, Cloudflare AI, and Ollama. Choose a
provider and configure its API key, model, and optional base URL in Settings.

## Architecture

- Mobile app: USB serial transport, metadata editor/import/export, cached
  manifest, and provider-independent AI.
- ARIA firmware library: compact versioned protocol, action and variable
  registration, dispatch, synchronization, and results.
- User sketch: hardware setup and device logic.
- Metadata: portable JSON/YAML, kept out of firmware.

## Quick start

```sh
nvm install 20 && nvm use 20
npm install
npm start
```

On Linux, choose **Build and run Electron app** in the launcher menu or run
`npm start -- -linux -r`. After Electron loads, press `r` in the same terminal
to rebuild the Whisper worker and Linux renderer from the current source, then
reload the app. Press `Ctrl+C` to quit. If the build fails, the current app
remains open and pressing `r` retries it.

## Arduino examples

The [simulated sensor sketch](artifacts/arduino/ARIA-library/examples/simulated_sensors/simulated_sensors.ino)
updates a simulated temperature every three seconds, registers `run` and
`get_greeting` actions and an `obstacle` event, and can send “hello” as a raw64
payload. The temperature varies between 22 °C and 28 °C.

For a simpler sensor example with matching metadata, use
[fake room temperature](artifacts/arduino/ARIA-library/examples/fake_room_temperature/fake_room_temperature.ino)
and its adjacent `aria.json`. Its simulated reading is synchronized every
three seconds.

Install the library by copying
`artifacts/arduino/ARIA-library` into your Arduino libraries folder:

- Linux: `~/Arduino/libraries/Aria`
- Windows: `Documents\Arduino\libraries\Aria`
- macOS: `~/Documents/Arduino/libraries/Aria`

Open an example sketch, select a board and serial port, then upload it. Use a
native Android build for USB serial; Expo Go cannot access the Android USB host
API.

## Firebase Analytics and Crashlytics

The mobile app uses React Native Firebase for native Analytics screen views and
Crashlytics reports. These services are app-only and do not add any Firebase
code to the Arduino library. Android is configured now; iOS telemetry remains
inactive until its Firebase plist is added. They do not run in Expo Go, web, or
the Linux/Electron app.

Create a Firebase project, register the app's Android ID
(`com.abhyanshac.aria`) and enable Analytics and Crashlytics in the Firebase
console. Place the Android configuration file at the mobile project root:

- Android: `artifacts/mobile/google-services.json`

When setting up iOS later, register the app's bundle ID
(`com.aria.voiceassistant`) and place `GoogleService-Info.plist` at
`artifacts/mobile/GoogleService-Info.plist`, then set `ios.googleServicesFile`
in the app config.

Build a new development client after adding the files and Firebase packages;
an already-installed client does not include the native Firebase modules. For
local native builds, regenerate and build with:

```sh
cd artifacts/mobile
npx expo prebuild --clean
npx expo run:android
# or: npx expo run:ios
```

On Android, initializing Crashlytics at app startup enables its JavaScript
exception and unhandled-rejection handlers. The app's React error boundary also
records caught rendering errors and their component stacks. Crash reporting is
disabled by default in development builds; use a release build to verify crash
reports.

## Test with physical hardware

Connect the flashed board over USB and run the serial integration test with its
device path. For an Uno with a CH340 adapter:

```sh
SERIAL_PORT=/dev/ttyUSB0 npm run test:serial
```

For boards exposed as CDC serial:

```sh
SERIAL_PORT=/dev/ttyACM0 npm run test:serial
```

The test uses 115200 baud and calls the configured AI provider after testing
the hardware protocol. Close Arduino IDE's Serial Monitor before running it.
If you change the library or sketch, upload the updated sketch first.

## Transport and protocol

The current transport is phone or desktop app ↔ USB Serial ↔ microcontroller.
Bluetooth, Wi-Fi, and MQTT are intentionally out of scope for the core
protocol. See the [ARIA v2 protocol](docs/ARIA_protocol.md) for the
application-to-firmware contract.

The repository is proprietary and closed source; access and redistribution are
controlled by the project owner.

## Arduino & App Metadata Simulation

### Arduino sketch
Flash this to a Arduino compatible Microcontroller with the 'Aria' library.

```cpp
#include <Arduino.h>
#include "Aria.h"

const unsigned long updateIntervalMs = 5000;
unsigned long lastUpdate = 0;

int soilMoisturePercent = 62;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // On an Uno, leave A0 unconnected to provide a varying seed.
  randomSeed(analogRead(A0));

  soilMoisturePercent = random(62, 71);

  ARIA.begin(Serial, "soil-moisture");
  ARIA.device("Simulated Soil Moisture", "1.0.0");
  ARIA.variable("soilMoisture", &soilMoisturePercent, 62, 70);

  lastUpdate = millis();
}

void loop() {
  ARIA.loop();

  const unsigned long now = millis();
  if (now - lastUpdate >= updateIntervalMs) {
    lastUpdate = now;
    soilMoisturePercent = random(62, 71);
    ARIA.sync("soilMoisture");
  }
}
```

### `aria.json`

Place this metadata file in Manage Profile -> Edit/Import -> Portable profile (v2) then paste this:

```json
{
  "schemaVersion": 2,
  "device": {
    "id": "soil-moisture",
    "name": "Simulated Soil Moisture",
    "protocolVersion": 2,
    "baudRate": 115200
  },
  "actions": [],
  "variables": [
    {
      "id": "soilMoisture",
      "name": "Soil Moisture",
      "aliases": ["moisture", "soil moisture", "soil humidity"],
      "description": "Simulated soil moisture percentage reported by the Arduino over USB.",
      "type": "int",
      "access": "readOnly",
      "unit": "%",
      "min": 62,
      "max": 70,
      "instructions": [
        "Report the latest soilMoisture runtime reading as a whole-number percentage.",
        "The Arduino generates a random integer from 62 through 70 every five seconds.",
        "This value is simulated, not measured by a physical moisture sensor."
      ],
      "category": "Sensors",
      "icon": "droplet"
    }
  ],
  "events": []
}
```
