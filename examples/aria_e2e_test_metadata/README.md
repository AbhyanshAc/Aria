# ARIA moisture + LED end-to-end test

This standalone example tests the full ARIA round trip:

1. ARIA sends an action to the Arduino to turn its built-in **L** LED off.
2. ARIA calls a registered webhook.
3. A small Python HTTP server receives the request and returns a JSON response.
4. ARIA interprets that response and calls the Arduino's `led_on` action.
5. The **L** LED turns back on.

The webhook server uses only Python's standard library. It replaces the MacroDroid part of the original test; it does not need to know anything about the Arduino or the ARIA serial protocol.

## 1. Upload the Arduino example

1. Open `aria_moisture_led_demo.ino` in the Arduino IDE.
2. Add `artifacts/arduino/ARIA-library` as an Arduino library, or copy it into your Arduino libraries folder.
3. Select your board and serial port, then upload the sketch.
   - The sketch uses `LED_BUILTIN`; on many Uno-compatible boards this is the onboard **L** LED on pin 13.
4. Connect the board to the device running ARIA over USB serial at **115200 baud**.
5. Import `aria.json` as the device profile in ARIA.
6. Confirm that the device ID is `moisture-led-demo`.

You can now ask ARIA to turn the onboard LED on or off. The example also publishes a simulated `moisturePct` value every three seconds; no moisture sensor is connected.

## 2. Create the Python webhook server

On the computer that will run the webhook, create a file named `webhook.py` with the following contents:

```python
from http.server import BaseHTTPRequestHandler, HTTPServer
import json

# The response tells ARIA to speak and then run the Arduino's led_on action.
RESPONSE = {
    "speech": "Request received. Turning the LED back on.",
    "actions": [
        {
            "id": "led_on",
            "args": []
        }
    ]
}


class Webhook(BaseHTTPRequestHandler):
    def do_GET(self):
        self.respond()

    def do_POST(self):
        self.respond()

    def respond(self):
        if self.path != "/webhook":
            self.send_error(404)
            return

        # Read and discard the request body, if one was sent.
        length = int(self.headers.get("Content-Length", 0))
        if length:
            self.rfile.read(length)

        data = json.dumps(RESPONSE).encode("utf-8")

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

        print("Webhook received!")


server = HTTPServer(("0.0.0.0", 8000), Webhook)
print("Webhook running on port 8000")
server.serve_forever()
```

Run it from a terminal:

```bash
python3 webhook.py
```

Keep this terminal open while testing. When ARIA calls the endpoint, the terminal prints:

```text
Webhook received!
```

The server listens on port `8000`, accepts both `GET` and `POST` requests at `/webhook`, and returns the same JSON response each time. It currently ignores any request body.

## 3. Register the webhook in ARIA

In the webhook registration page, add an entry with these values:

| Field | Value |
|---|---|
| Webhook ID | `led_webhook_cycle` |
| Display name | `LED Webhook Cycle` |
| Description | `Call this webhook after turning the LED off. It confirms the request and returns an instruction to turn the LED back on.` |
| Webhook URL | `http://YOUR_PC_IP:8000/webhook` |
| Method | `POST` |

Replace `YOUR_PC_IP` with the IP address of the computer running `webhook.py`.

On Linux, find the computer's local IP address with:

```bash
hostname -I
```

For example, if the computer's address is `192.168.1.10`, use:

```text
http://192.168.1.10:8000/webhook
```

The webhook ID must match the `led_webhook_cycle` integration entry in `aria.json`.

### Choose a URL ARIA can reach

- **ARIA and Python server on the same device:** use `http://localhost:8000/webhook`.
- **ARIA on another device on the same Wi-Fi/LAN:** use the Python computer's LAN IP, such as `http://192.168.1.10:8000/webhook`. Both devices must be on the same network, and the computer's firewall must allow inbound connections to port `8000`.
- **ARIA running remotely or in the cloud:** a private LAN address and `localhost` will not be reachable. Use a publicly reachable HTTPS endpoint or a secure tunnel that exposes the local server.

Do not expose this example server directly to the public internet. It has no authentication or TLS of its own; use a protected HTTPS reverse proxy or authenticated tunnel if remote access is required.

## 4. Run the end-to-end test

1. Make sure the Arduino is connected to ARIA and the device profile is loaded.
2. Start the Python server with `python3 webhook.py`.
3. Confirm the webhook registration is saved in ARIA.
4. Ask ARIA:

   > Turn the LED off, then call the LED Webhook Cycle action.

5. Watch the Arduino's **L** LED and the Python terminal.

Expected result:

- The **L** LED turns off.
- The Python terminal prints `Webhook received!`.
- ARIA receives the webhook's JSON response.
- ARIA says, “Request received. Turning the LED back on.”
- The **L** LED turns back on.

This confirms the complete AI/action → webhook → response/action round trip.

## 5. Troubleshooting

- **No `Webhook received!` message:** check that the Python process is still running and that the registered URL uses the correct computer IP and port.
- **ARIA cannot connect:** make sure both devices are on the same network, check the computer's firewall, and verify that the URL is reachable from the device running ARIA.
- **The webhook is called but the LED does not turn on:** check that the Arduino is still connected, that the imported profile contains the `led_on` action, and that the response JSON uses the action ID `led_on`.
- **404 response:** the endpoint path must be exactly `/webhook`.
- **Port already in use:** stop the other process using port `8000`, or change the port in both `webhook.py` and the registered URL.

## 6. What this example demonstrates

The Arduino sketch exposes two firmware actions:

- `led_off` — turns the built-in LED off.
- `led_on` — turns the built-in LED on.

The profile registers `led_webhook_cycle` as the Android webhook integration action. The Python server does not control the Arduino directly. It simply returns a response containing the `led_on` action, which ARIA executes through the connected device.

The moisture reading is simulated test data. The sketch intentionally does not execute arbitrary AI-generated text as a command; LED changes happen through the explicitly registered firmware actions.
