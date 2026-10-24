#include <Arduino.h>
#include "Aria.h"

float temperatureC = 22.5f;
unsigned long lastUpdate = 0;
const unsigned long updateIntervalMs = 3000;
const float roomTemperatureC = 22.5f;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  randomSeed(analogRead(A0));
  temperatureC = random(215, 236) / 10.0f;

  ARIA.begin(Serial, "fake-room-temp");
  ARIA.device("Simulated Room Temperature", "1.0.0");
  ARIA.variable("temperatureC", &temperatureC, 20.0f, 26.0f);
}

void loop() {
  ARIA.loop();

  const unsigned long now = millis();
  if (now - lastUpdate >= updateIntervalMs) {
    lastUpdate = now;

    // Slowly drift with small noise, while tending back toward a room-temperature baseline.
    temperatureC += (roomTemperatureC - temperatureC) * 0.04f;
    temperatureC += random(-10, 11) / 100.0f;
    temperatureC = constrain(temperatureC, 20.0f, 26.0f);
    ARIA.sync("temperatureC");
  }
}
