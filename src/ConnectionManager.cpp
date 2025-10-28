/*
 * ConnectionManager.cpp
 * Implementation file for the ConnectionManager library.
 */

// This specific include order is required to solve compile errors
#include <Arduino.h>
#include <functional>
#include <PubSubClient.h>

#include "config.h" // Project-wide settings and constants
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

// Set the callback for MQTT messages
void ConnectionManager::setCallback(MQTT_CALLBACK_SIGNATURE) {
  this->mqttClient.setCallback(callback);
}

// Reconnect logic for MQTT, now non-blocking
void ConnectionManager::reconnect() {
  Serial.print("Attempting MQTT connection...");
  
  // Attempt to connect using the stored device token
  if (mqttClient.connect("ESP32Client", this->device_token, NULL)) {
    Serial.println("connected");
    
    // Subscribe to RPC requests
    mqttClient.subscribe(rpc_topic);
    Serial.println("Subscribed to RPC topic");

    // We must ALSO subscribe to shared attribute updates for OTA
    mqttClient.subscribe(attribute_topic);
    Serial.println("Subscribed to Shared Attributes topic");

    // Subscribe to the response topic for attribute requests
    mqttClient.subscribe(attributes_resp_topic);
    Serial.println("Subscribed to Attributes Response topic");

  } else {
    Serial.print("failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" try again in 5 seconds");
    // Note: The delay is now handled in the main loop()
  }
}

// Main loop: keeps the client connected and handles reconnects
void ConnectionManager::loop() {
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    // Check if MQTT_RECONNECT_WAIT_MS has passed since last attempt
    if (now - lastReconnectAttempt > MQTT_RECONNECT_WAIT_MS) {
      lastReconnectAttempt = now;
      reconnect();
    }
  }
  // mqttClient.loop() should be called continuously to process messages
  // and maintain the connection.
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

/**
 * @brief Publishes a request for shared attributes.
 * Assumes connection and subscription are already handled.
 */
void ConnectionManager::requestAttributes() {
  JsonDocument doc;
  // We only care about shared keys for OTA
  doc["sharedKeys"] = "fw_version,fw_title"; 
  
  char json_payload[100];
  serializeJson(doc, json_payload);

  Serial.println("Requesting shared attributes from server...");
  
  // Publish to the request topic with ID 1
  // (Subscription is handled in reconnect())
  mqttClient.publish(attributes_req_topic, json_payload);
}


// --- OTA FUNCTION (PULL METHOD - C-String ONLY) ---
// Performs the Over-The-Air update using memory-safe C-strings
void ConnectionManager::performOtaUpdate(const char* title, const char* version) {
  
  // 1. URL-encode title and version into temporary, stack-allocated buffers.
  // This is a safe way to handle it without using the String class, which can cause heap fragmentation.
  char title_encoded[128];
  char version_encoded[64];

  // Simple URL-encoding for spaces (' ' -> '%20')
  char* t_out = title_encoded;
  for (const char* t_in = title; *t_in && t_out < title_encoded + sizeof(title_encoded) - 4; t_in++) {
    if (*t_in == ' ') { *t_out++ = '%'; *t_out++ = '2'; *t_out++ = '0'; }
    else { *t_out++ = *t_in; }
  }
  *t_out = '\0';

  char* v_out = version_encoded;
  for (const char* v_in = version; *v_in && v_out < version_encoded + sizeof(version_encoded) - 4; v_in++) {
    if (*v_in == ' ') { *v_out++ = '%'; *v_out++ = '2'; *v_out++ = '0'; }
    else { *v_out++ = *v_in; }
  }
  *v_out = '\0';

  // 2. Build the URL as a C-style string with the token in the path
  // FORMAT: https://{host}/api/v1/{token}/firmware?title={title}&version={version}
  char url[512]; // Use a fixed, larger buffer for safety
  snprintf(url, sizeof(url), "https://%s/api/v1/%s/firmware?title=%s&version=%s",
           this->http_host,
           this->device_token,
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

