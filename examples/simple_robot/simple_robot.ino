#include <Arduino.h>
#include "Aria.h"

int cruiseSpeed = 100;
bool obstacleDetected = false;

void onRun(const char *payload) {
  // payload example: "i:120" -> parse the supplied value.
  int value = 0;
  if (strncmp(payload, "i:", 2) == 0) {
    value = atoi(payload + 2);
  }
  
  // Perform user hardware logic here.
  // We can't use Serial.print since ARIA takes over the serial port, 
  // but we return the result via ARIA.result
  ARIA.result("run", true);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  // 1. Initialize ARIA on the serial port with device ID "simple-robot"
  // Note: ARIA v2 protocol is default
  ARIA.begin(Serial, "simple-robot");
  
  // 2. Identify the device for the manifest
  ARIA.device("Simple Robot", "1.0.0");
  
  // 3. Register actions
  ARIA.registerAction("run", onRun);
  
  // 4. Register variables
  ARIA.variable("cruiseSpeed", &cruiseSpeed, 0, 255);
  
  // 5. Register events
  ARIA.event("obstacle");
}

unsigned long lastCheck = 0;

void loop() {
  // 1. Must call ARIA.loop() frequently to process incoming messages
  ARIA.loop();
  
  // 2. Simulate hardware event (e.g. ultrasonic sensor detecting obstacle)
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    
    // Randomly trigger obstacle event 10% of the time for demonstration
    if (random(10) == 0) {
      if (!obstacleDetected) {
        obstacleDetected = true;
        // Raise event to phone. If phone is in "auto" mode, the AI will handle it.
        ARIA.raiseEvent("obstacle", "Obstacle detected ahead!");
      }
    } else {
      obstacleDetected = false;
    }
  }
  
  delay(10);
}
