# ARIA moisture + LED test

This is a standalone example; it does not modify the Aria application. It uses
the ARIA Arduino library already in `artifacts/arduino/ARIA-library`.

## Upload and connect

1. Open `aria_moisture_led_demo.ino` in Arduino IDE and add
   `artifacts/arduino/ARIA-library` as an Arduino library (or copy it into your
   Arduino libraries folder).
2. Select your board/port and upload. The sketch assumes a board with
   `LED_BUILTIN`; common Uno boards use pin 13.
3. In ARIA, connect to the board over USB serial at **115200 baud**, then import
   `aria.json` as the device profile. The profile's device ID must match the
   sketch's `moisture-led-demo` ID.
4. Ask ARIA to turn the LED on/off, or ask what the device exposes. Firmware
   sends the AI question after connection time and retries until it receives an
   answer. Moisture readings are simulated and pushed every three seconds.

## MacroDroid webhook test

Create an ARIA MacroDroid preset/action with ID `led_webhook_cycle` and configure
its trigger URL in ARIA's Plugins settings. Add that ID to this profile's
`integrations.android.macrodroid.actions` list (already present here). Create a
MacroDroid macro triggered by the corresponding HTTP request. Have its response
return this JSON after receiving the request:

```json
{
  "speech": "MacroDroid received the LED-off request and is turning the LED back on.",
  "actions": [{ "id": "led_on", "args": [] }]
}
```

Test sequence: ask ARIA to turn the LED off; its registered `led_off` firmware
action turns it off, and the ARIA webhook notification invokes MacroDroid. The
webhook response then asks ARIA to invoke `led_on`, which turns it back on.
This tests the AI-to-board action plus a webhook-response action back to the
board. MacroDroid must be able to return the JSON response through the configured
endpoint for this round trip; a MacroDroid trigger by itself does not directly
write to the Arduino's USB serial connection.

The Arduino's `Q`/`P` support is separate from webhooks: the sketch asks ARIA's
configured AI to explain the demo, and ARIA returns the answer to firmware. The
sketch intentionally does not execute AI text as a command; LED changes only
come from the registered `led_on` and `led_off` actions.
