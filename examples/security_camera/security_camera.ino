#include <Arduino.h>
#include "Aria.h"

bool armed = true;
String lastSnapshot = ""; // In a real sketch, this would be a buffer of bytes

void onArm(const char *payload) {
  armed = true;
  ARIA.result("arm", true);
  ARIA.sync("armedStatus");
}

void onDisarm(const char *payload) {
  armed = false;
  ARIA.result("disarm", true);
  ARIA.sync("armedStatus");
}

void onCaptureSnapshot(const char *payload) {
  // Simulate capturing an image and getting a small binary payload (e.g. 10x10 BMP)
  uint8_t dummyImage[] = { 0x42, 0x4D, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  
  // Send back as base64 raw64 binary data to the 'snapshot' variable
  ARIA.sendRaw64("snapshot", dummyImage, sizeof(dummyImage));
  ARIA.result("captureSnapshot", true);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  ARIA.begin(Serial, "security-camera");
  ARIA.device("Smart Security Camera", "1.0.0");
  
  ARIA.registerAction("arm", onArm);
  ARIA.registerAction("disarm", onDisarm);
  ARIA.registerAction("captureSnapshot", onCaptureSnapshot);
  
  ARIA.variable("armedStatus", &armed);
  ARIA.variableRaw64("snapshot", &lastSnapshot);
  
  ARIA.event("motionDetected");
}

unsigned long lastMotionCheck = 0;

void loop() {
  ARIA.loop();
  
  if (millis() - lastMotionCheck > 10000) {
    lastMotionCheck = millis();
    
    // Simulate motion detection when armed
    if (armed && random(5) == 0) {
      ARIA.raiseEvent("motionDetected", "Motion detected in the living room!");
    }
  }
  
  delay(10);
}
