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
#define FIRMWARE_VERSION "1.1.6"
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
// NOTE: This is NO LONGER USED in the deep-sleep version.
// The new SLEEP_DURATION_S is the main interval.
const unsigned long SEND_INTERVAL_MS = 10000;


// --- DEEP SLEEP SETTINGS ---
// This is the new "interval". How long to sleep between readings (in seconds).
// 600 seconds = 10 minutes
// 3600 seconds = 1 hour
const uint64_t SLEEP_DURATION_S = 30; 
    
// Convert seconds to microseconds for the timer
#define uS_TO_S_FACTOR 1000000ULL 
#define TIME_TO_SLEEP_US (SLEEP_DURATION_S * uS_TO_S_FACTOR) 


#endif // CONFIG_H
