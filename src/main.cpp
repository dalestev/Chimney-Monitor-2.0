/*
 * main.cpp
 * Main project file for ESP32 ThingsBoard Client
 */

#include <Arduino.h>
#include "config.h"
#include "ConnectionManager.h"
#include <ArduinoJson.h>
#include "BatteryManager.h"
#include "ShtManager.h"
#include "ChimneyProbe.h"

// Initialize our ConnectionManager
ConnectionManager conn(TB_MQTT_HOST, TB_MQTT_PORT, TB_HTTP_HOST);


//Init BatteryManager
BatteryManager batt;


//  Init SHT30 Manager
ShtManager sht;


// Init ChimneyProbe
ChimneyProbe chimney;

// Timer for sending data
unsigned long lastSend = 0;

// Flag to track if we've sent initial attributes
bool attributes_sent = false;

// --- MQTT Message Callback ---
// This function is called every time a message arrives
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  Serial.println("---");
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  // Copy payload to a new buffer to null-terminate it
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  Serial.print("Message: ");
  Serial.println(message);

  // Check if this is a shared attribute update (for OTA)
  if (strcmp(topic, "v1/devices/me/attributes") == 0) {
    JsonDocument doc;
    deserializeJson(doc, message);

    // Check if the message contains new firmware info
    // Use the modern ArduinoJson 7 check:
    if (doc["fw_version"].is<const char*>()) {
      const char* fw_title = doc["fw_title"];
      const char* fw_version = doc["fw_version"];
      
      Serial.printf("Received new firmware info. Title: %s, Version: %s\n", fw_title, fw_version);

      // Compare with our current version
      if (strcmp(fw_version, FIRMWARE_VERSION) != 0) {
        Serial.println("New firmware detected! Starting OTA update process...");
        // Call performOtaUpdate without the token, as the class now stores it
        conn.performOtaUpdate(fw_title, fw_version);
      } else {
        Serial.println("Firmware is already up to date.");
      }
    }
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for serial
  }

  Wire.begin();

  if (batt.begin(&Wire, LC709203F_APA_500MAH)) {
    Serial.println("Battery monitor initialized.");
  } else {
    Serial.println("WARNING: Battery monitor not found!");
  }


  if (sht.begin()) {
    Serial.println("SHT30 sensor initialized. Warming up...");
    Serial.printf("Initial Ambient Temp: %.2f F", sht.getTemperature());

  } else {
    Serial.println("WARNING: SHT30 sensor not found!");
  }

  Serial.println("Chimney Probe Initialized...Delaying First Read");
  delay(500);
  Serial.printf("Initial Chimney Temp: %.2f F\n", chimney.getTemperature());


  // Connect to Wi-Fi and ThingsBoard
  conn.connectWifi(WIFI_SSID, WIFI_PASS);
  
  // Pass the token to be stored in the class
  conn.connectThingsboard(TB_DEVICE_TOKEN); 
  
  // Set our callback function
  conn.setCallback(onMqttMessage); 
}

// --- Main Loop ---
void loop() {
  // Main loop for the connection manager (handles MQTT keep-alive)
  conn.loop();

  // Send initial attributes ONCE after connecting
  if (conn.isConnected() && !attributes_sent) {
    JsonDocument doc;
    doc["fw_title"] = FIRMWARE_TITLE;
    doc["fw_version"] = FIRMWARE_VERSION;
    doc["fw_state"] = "IDLE"; // Report that we are ready
    
    char json_payload[150];
    serializeJson(doc, json_payload);
    
    conn.sendAttributes(json_payload);
    attributes_sent = true;
  }

  // Send telemetry data every SEND_INTERVAL_MS
  if (millis() - lastSend > SEND_INTERVAL_MS && conn.isConnected()) {
    lastSend = millis();

    // Create a JSON payload for telemetry
    JsonDocument telemetry_doc;
    
    // --- Telemetry Section Updated ---
    
    // Remove mock data
    // telemetry_doc["temperature"] = random(18, 25) + random(0, 100) / 100.0f; 
    // telemetry_doc["humidity"] = random(40, 60) + random(0, 100) / 100.0f;    

    // Battery Data
    telemetry_doc["batt_voltage"] = batt.getVoltage();
    telemetry_doc["batt_percent"] = batt.getPercentage();

    // SHT30 "External" Data
    telemetry_doc["ext_temp"] = sht.getTemperature();
    telemetry_doc["ext_hum"] = sht.getHumidity();

    // Connection Data
    telemetry_doc["rssi"] = conn.getRSSI();

    // Add the REAL Chimney Temperature
    float chimneyTemp = chimney.getTemperature();
    // Only add to JSON if the reading is valid (not NAN)
    if (!isnan(chimneyTemp)) {
      telemetry_doc["chimney_temp"] = chimneyTemp;
    }
    // --- End of Update ---

    char json_payload[300]; // Increased buffer size slightly
    serializeJson(telemetry_doc, json_payload);
    
    Serial.println("Sending telemetry...");
    conn.sendTelemetryJson(json_payload);
  }
}

