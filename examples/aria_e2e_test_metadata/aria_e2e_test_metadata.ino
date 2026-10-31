#include <Arduino.h>
#include "Aria.h"

// Standalone ARIA v2 demo: simulated moisture telemetry, phone LED commands,
// and an asynchronous question to the phone's configured AI.
const uint8_t LED_PIN = LED_BUILTIN;
float moisturePct = 42.0f;
bool ledOn = false;
bool aiAnswered = false;
unsigned long lastMoistureUpdate = 0;
unsigned long lastAiRequest = 0;

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void onLedOn() {
  setLed(true);
  ARIA.result("led_on", true);
}

void onLedOff() {
  setLed(false);
  ARIA.result("led_off", true);
}

void onAIResponse(const char *requestId, const char *response) {
  if (strcmp(requestId, "intro") == 0) {
    aiAnswered = true;
    // ARIA routes the response to the app; don't print to Serial because it
    // carries the framed protocol.
    (void)response;
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
  Serial.begin(115200);

  ARIA.begin(Serial, "moisture-led-demo");
  ARIA.device("Moisture and LED Demo", "1.0.0");
  ARIA.registerAction("led_on", onLedOn);
  ARIA.registerAction("led_off", onLedOff);
  ARIA.variable("moisturePct", &moisturePct, 0.0f, 100.0f);
  ARIA.variable("ledOn", &ledOn);
  ARIA.onAIResponse(onAIResponse);
  randomSeed(analogRead(A0));
}

void loop() {
  ARIA.loop();
  const unsigned long now = millis();

  // Fake moisture reading gently wanders between 25 and 75 percent.
  if (now - lastMoistureUpdate >= 3000UL) {
    lastMoistureUpdate = now;
    moisturePct += random(-30, 31) / 10.0f;
    if (moisturePct < 25.0f) moisturePct = 25.0f;
    if (moisturePct > 75.0f) moisturePct = 75.0f;
    ARIA.sync("moisturePct");
  }

  // Ask once the serial app has had time to connect. Retry until answered.
  if (!aiAnswered && now >= 5000UL && now - lastAiRequest >= 15000UL) {
    lastAiRequest = now;
    ARIA.askAI("Explain what this moisture and LED demo exposes, and clarify that moisturePct is simulated.", "intro");
  }
}
