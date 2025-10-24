/*
 * ConnectionManager.cpp
 * Implementation file for the ConnectionManager library.
 */

// This specific include order is required to solve compile errors
#include <Arduino.h>
#include <functional>
#include <PubSubClient.h>

#include "ConnectionManager.h"
#include <ArduinoJson.h> 
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h> // Include the HTTPS client

// Helper function to send fw_state updates
// We remove the default argument from the .cpp file to fix the compile error
void ConnectionManager::sendFwState(const char* state, const char* error) {
  JsonDocument doc;
  doc["fw_state"] = state;
  if (strlen(error) > 0) {
    doc["fw_error"] = error;
  }
  char json_payload[150];
  serializeJson(doc, json_payload);
  sendAttributes(json_payload);
}

// Constructor: Initializes the MQTT client with server details
ConnectionManager::ConnectionManager(const char* mqtt_host, int mqtt_port, const char* http_host) 
  : tb_host(mqtt_host), http_host(http_host), tb_port(mqtt_port), mqttClient(espClient) {
  
  // Set the MQTT server and port
  mqttClient.setServer(tb_host, tb_port);
}

// Connects to Wi-Fi
void ConnectionManager::connectWifi(const char* wifi_ssid, const char* wifi_pass) {
  this->wifi_ssid = wifi_ssid;
  this->wifi_pass = wifi_pass;

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Connects to ThingsBoard MQTT broker
void ConnectionManager::connectThingsboard(const char* device_token) {
  // Store the device token as a class member
  this->device_token = device_token;
  Serial.println("Connecting to ThingsBoard...");
  reconnect(); // Perform the initial connection
}

// Set the callback for MQTT messages (no extra "callback" variable)
// This implementation is correct.
void ConnectionManager::setCallback(MQTT_CALLBACK_SIGNATURE) {
  this->mqttClient.setCallback(callback);
}

// Reconnect logic for MQTT
void ConnectionManager::reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Attempt to connect using the stored device token
    if (mqttClient.connect("ESP32Client", this->device_token, NULL)) {
      Serial.println("connected");
      
      // Subscribe to RPC requests
      mqttClient.subscribe(rpc_topic);
      Serial.println("Subscribed to RPC topic");

      // We must ALSO subscribe to shared attribute updates for OTA
      mqttClient.subscribe(attributes_sub_topic);
      Serial.println("Subscribed to Shared Attributes topic");

    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Main loop: keeps the client connected
void ConnectionManager::loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
}

// Check if MQTT is connected
bool ConnectionManager::isConnected() {
  return mqttClient.connected();
}

// Send a simple key-value pair
bool ConnectionManager::sendTelemetry(const char* key, float value) {
  JsonDocument doc;
  doc[key] = value;
  char json_payload[100];
  serializeJson(doc, json_payload);
  return sendTelemetryJson(json_payload);
}

// Send a pre-formatted JSON payload
bool ConnectionManager::sendTelemetryJson(const char* json_payload) {
  Serial.print("Sending telemetry: ");
  Serial.println(json_payload);
  return mqttClient.publish(telemetry_topic, json_payload);
}

// Send a client-side attribute
bool ConnectionManager::sendAttributes(const char* json_payload) {
  Serial.print("Sending attributes: ");
  Serial.println(json_payload);
  return mqttClient.publish(attribute_topic, json_payload);
}

// --- OTA FUNCTION (PULL METHOD - C-String ONLY) ---
// Performs the Over-The-Air update
void ConnectionManager::performOtaUpdate(String title, String version) {
  
  // 1. URL-encode title and version.
  String title_encoded_str = title;
  title_encoded_str.replace(" ", "%20");
  String version_encoded_str = version;
  version_encoded_str.replace(" ", "%20");

  const char* title_encoded = title_encoded_str.c_str();
  const char* version_encoded = version_encoded_str.c_str();

  // 2. Build the URL as a C-style string with the token in the path
  // FORMAT: https://{host}/api/v1/{token}/firmware?title={title}&version={version}
  size_t urlLen = strlen("https://") + strlen(this->http_host) + strlen("/api/v1/") + strlen(this->device_token) + strlen("/firmware?title=") + strlen(title_encoded) + strlen("&version=") + strlen(version_encoded) + 1;
  char url[urlLen];
  snprintf(url, urlLen, "https://%s/api/v1/%s/firmware?title=%s&version=%s",
           this->http_host,
           this->device_token, // <-- TOKEN IS NOW PART OF THE URL
           title_encoded,
           version_encoded);
  
  Serial.println("Starting OTA update from: ");
  Serial.println(url);
  
  // 3. Report DOWNLOADING state
  sendFwState("DOWNLOADING");
  
  HTTPClient http;
  WiFiClientSecure clientSecure;

  // WARNING: Insecure! Skipping certificate validation.
  clientSecure.setInsecure();

  // Use the secure client and the char* URL
  if (!http.begin(clientSecure, url)) {
    Serial.println("Failed to initialize https.begin()");
    sendFwState("FAILED", "HTTPS begin failed");
    return;
  }
  
  // 4. Add the Accept header (still required)
  http.addHeader("Accept", "application/octet-stream");
  
  // 5. Make the request (No X-Authorization header needed)
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed, error code: %d\n", httpCode);
    http.end();
    sendFwState("FAILED", "HTTP GET failed");
    return;
  }

  // 6. Check content length
  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("Content length error");
    http.end();
    sendFwState("FAILED", "Content length error");
    return;
  }

  Serial.printf("Got update. Size: %d\n", contentLength);

  // 7. Begin update process
  if (!Update.begin(contentLength)) {
    Serial.println("Not enough space to begin OTA");
    Update.printError(Serial);
    http.end();
    sendFwState("FAILED", "Not enough space");
    return;
  }

  // 8. Report DOWNLOADED state
  sendFwState("DOWNLOADED");
  Serial.println("Download complete. Applying update...");
  mqttClient.loop(); // Give the client time to send the message
  delay(100);

  // Get the stream from the HTTP response
  WiFiClient& stream = http.getStream();

  // 9. Write the stream to the update partition
  Serial.println("Writing update...");
  size_t written = Update.writeStream(stream);

  if (written != contentLength) {
    Serial.printf("Written only %d of %d bytes! Aborting!\n", written, contentLength);
    Update.end(false); // Abort
    http.end();
    sendFwState("FAILED", "Incomplete write");
    return;
  }

  // 10. Report UPDATING state
  sendFwState("UPDATING");
  Serial.println("Write complete. Finalizing update...");
  mqttClient.loop(); // Give the client time to send the message
  delay(100);

  // 11. Finalize the update
  if (!Update.end(true)) {
    Serial.println("Error ending update");
    Update.printError(Serial);
    http.end();
    sendFwState("FAILED", "Update.end() error");
    return;
  }

  // 12. Success
  Serial.println("Update successful! Restarting...");
  http.end();
  ESP.restart();
}

long ConnectionManager::getRSSI() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.RSSI();
    }
    return 0; // Return 0 if not connected
}