#include <Arduino.h>
#include "Aria.h"

float temperature = 22.5;
float humidity = 45.0;
bool heaterOn = false;

void onToggleHeater(const char *payload) {
  // Toggle the heater state
  heaterOn = !heaterOn;
  // Let the assistant know the operation was successful
  ARIA.result("toggleHeater", true);
  
  // Sync the new state back to the phone
  ARIA.sync("heaterState");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  ARIA.begin(Serial, "weather-station");
  ARIA.device("Smart Weather Station", "1.0.0");
  
  ARIA.registerAction("toggleHeater", onToggleHeater);
  
  // Register read-only sensor variables
  ARIA.variable("temperature", &temperature, -40, 80);
  ARIA.variable("humidity", &humidity, 0, 100);
  
  // Register a state variable
  ARIA.variable("heaterState", &heaterOn);
  
  // Register events
  ARIA.event("frostWarning");
}

unsigned long lastUpdate = 0;

void loop() {
  ARIA.loop();
  
  // Simulate sensor readings changing over time
  if (millis() - lastUpdate > 2000) {
    lastUpdate = millis();
    
    // Slight random variations
    temperature += (random(-10, 11) / 10.0);
    humidity += (random(-5, 6) / 10.0);
    
    // Constrain to reasonable bounds
    temperature = constrain(temperature, -10, 40);
    humidity = constrain(humidity, 20, 90);
    
    // Apply heater effect
    if (heaterOn) {
      temperature += 0.5;
      humidity -= 0.2;
    }
    
    // Frost warning event
    if (temperature < 0 && !heaterOn) {
      ARIA.raiseEvent("frostWarning", "Temperature dropped below freezing!");
    }
  }
}
