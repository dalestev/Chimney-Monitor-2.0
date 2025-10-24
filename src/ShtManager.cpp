/*
 * ShtManager.cpp
 * Implementation for the SHT30/SHT31 sensor library.
 */

#include "ShtManager.h"

ShtManager::ShtManager() : sht(Adafruit_SHT31()), is_initialized(false) {
    // Constructor
}

// FIX: Removed TwoWire* parameter.
bool ShtManager::begin(uint8_t i2c_addr) {
    // The Adafruit SHT31 library's begin() only takes the address
    // and automatically uses the global 'Wire' instance.
    // The user MUST call Wire.begin() in setup() before this.
    
    // FIX: Removed sht.setWire(wireInstance); as it does not exist.
    if (!sht.begin(i2c_addr)) {
        Serial.println(F("ERROR: SHT30 sensor not found."));
        is_initialized = false;
        return false;
    }

    Serial.println(F("SHT30 sensor found."));
  
    is_initialized = true;
    return true;
}

float ShtManager::getTemperature() {
    if (!is_initialized) {
        return NAN; // Return Not-a-Number (NAN) on error
    }

    // Read temperature in Celsius first
    float tempC = sht.readTemperature();

    // FIX: Check for BOTH NaN and the -45C error code
    if (isnan(tempC) || tempC == -45.0) {
        Serial.println(F("ERROR: Failed to read temperature from SHT30"));
        return NAN; // Return Not-a-Number (NAN) on error
    }
    
    // Convert to Fahrenheit
    float tempF = (tempC * 1.8) + 32;
    return tempF;
}

float ShtManager::getHumidity() {
    if (!is_initialized) {
        return NAN; // Return Not-a-Number (NAN) on error
    }

    float hum = sht.readHumidity();

    // FIX: Check for NaN or any negative value (which indicates an error)
    if (isnan(hum) || hum < 0) {
        Serial.println(F("ERROR: Failed to read humidity from SHT30"));
        return NAN; // Return Not-a-Number (NAN) on error
    }
    
    return hum;
}

