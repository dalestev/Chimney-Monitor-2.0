/*
 * config.h
 *
 * This file contains all the user-configurable settings
 * for the ESP32 ThingsBoard project.
 */




/*
    Thingsboard

    What's next:    10/23/25
        - Chimney Probe
        - Sending Intervals
        - Read mAH usage



*/




#ifndef CONFIG_H
#define CONFIG_H

// Firmware Version
#define FIRMWARE_VERSION "1.1.4"
#define FIRMWARE_TITLE "Test Firmware"

// --- Wi-Fi Credentials ---
const char* WIFI_SSID = "Stev125";
const char* WIFI_PASS = "theletterRed7#!";


// --- ThingsBoard Professional Cloud details ---
// The host is "mqtt.thingsboard.cloud"
const char* TB_MQTT_HOST = "mqtt.thingsboard.cloud";
const int   TB_MQTT_PORT = 1883; 
#define TB_HTTP_HOST "thingsboard.cloud"

// --- Device Credentials ---
const char* TB_DEVICE_TOKEN = "yvwb2bf3s1pxkzgwtudc";



// --- Data Sending Interval ---
// How often to send data to ThingsBoard (in milliseconds)
const unsigned long SEND_INTERVAL_MS = 10000;


#endif // CONFIG_H
