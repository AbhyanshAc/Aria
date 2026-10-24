#include <Arduino.h>
#include "Aria.h"

// ── Realistic Simulated Sensors ─────────────────────────────────────────────
// Temperature fluctuates realistically around 24.5 °C (room temperature)
float temperature = 24.5;

// State variable
int speed = 0;

// Action handler: runs movement command
void onRun(const char *payload) {
  int val = 100;
  if (strncmp(payload, "i:", 2) == 0) {
    val = atoi(payload + 2);
  }
  speed = val;
  // Acknowledge action execution
  ARIA.result("run", true);
}

// Action handler: transmits the word "hello" in base64 / raw64
void onGetGreeting(const char *payload) {
  const char *msg = "hello";
  ARIA.sendRaw64("greeting", (const uint8_t*)msg, strlen(msg));
  ARIA.result("get_greeting", true);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  // 1. Initialize ARIA protocol over USB Serial
  ARIA.begin(Serial, "aria-sensor-board");
  ARIA.device("ARIA Simulated Sensor Device", "1.0.0");

  // 2. Register Actions
  ARIA.registerAction("run", onRun);
  ARIA.registerAction("get_greeting", onGetGreeting);

  // 3. Register Variables — temperature only; greeting is generated on demand
  ARIA.variable("temperature", &temperature, 0.0, 100.0);

  // 4. Register Events
  ARIA.event("obstacle");
}

unsigned long lastTempUpdate = 0;
unsigned long lastEventCheck = 0;

void loop() {
  // Keep ARIA serial communication active
  ARIA.loop();

  unsigned long now = millis();

  // Every 3 seconds, simulate realistic minor temperature fluctuation (+/- 0.2 °C)
  if (now - lastTempUpdate >= 3000) {
    lastTempUpdate = now;
    float delta = ((float)random(-20, 21)) / 100.0;
    temperature += delta;
    if (temperature < 22.0) temperature = 22.0;
    if (temperature > 28.0) temperature = 28.0;
  }

  // Periodic obstacle event demonstration (1 out of 5 checks every 15s)
  if (now - lastEventCheck >= 15000) {
    lastEventCheck = now;
    if (random(0, 5) == 0) {
      ARIA.raiseEvent("obstacle", "front");
    }
  }
}
