/*
 * ConnectionManager.h
 * Header file for the ConnectionManager library.
 * Manages Wi-Fi and ThingsBoard MQTT connections.
 */

#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

// This is required FIRST to solve the MQTT_CALLBACK_SIGNATURE compile error
#include <Arduino.h>

#include <functional> // Required for std::function
#include <WiFi.h>
#include <PubSubClient.h>

class ConnectionManager {
public:
  // Constructor
  ConnectionManager(const char* mqtt_host, int mqtt_port, const char* http_host);

  // Connection methods
  void connectWifi(const char* wifi_ssid, const char* wifi_pass);
  void connectThingsboard(const char* device_token);
  void loop();
  bool isConnected();
  void setCallback(MQTT_CALLBACK_SIGNATURE);

  // Data sending methods
  bool sendTelemetry(const char* key, float value);
  bool sendTelemetryJson(const char* json_payload);
  bool sendAttributes(const char* json_payload);

  // OTA methods
  void sendFwState(const char* state, const char* error = "");
  void performOtaUpdate(String title, String version);

  // Connection Status
  long getRSSI();

private:
  // Internal methods
  void reconnect();

  // Member variables
  const char* tb_host;
  const char* http_host;
  int tb_port;
  const char* wifi_ssid;
  const char* wifi_pass;
  const char* device_token; // Stored token

  // Topics
  const char* telemetry_topic = "v1/devices/me/telemetry";
  const char* attribute_topic = "v1/devices/me/attributes";
  const char* rpc_topic = "v1/devices/me/rpc/request/+";
  const char* attributes_sub_topic = "v1/devices/me/attributes";

  // Clients
  WiFiClient espClient;
  PubSubClient mqttClient;
};

#endif // CONNECTION_MANAGER_H

