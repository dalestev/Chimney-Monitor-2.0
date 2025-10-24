/*
 * BatteryManager.cpp
 * Implementation for the battery monitoring library.
 */

#include "BatteryManager.h"

BatteryManager::BatteryManager() : is_initialized(false) {
    // Constructor is empty
}

// FIX: Changed 'lc709203f_packsize_t' to 'lc709203_adjustment_t'
bool BatteryManager::begin(TwoWire *wireInstance, lc709203_adjustment_t packSize) {
    // Note: The caller (main.cpp) is responsible for calling Wire.begin()
    
    if (!lc.begin(wireInstance)) {
        Serial.println(F("ERROR: Battery Manager (LC709203F) not found."));
        is_initialized = false;
        return false;
    }

    Serial.println(F("Battery Manager (LC709203F) found."));

    // Set the battery pack size. This is crucial for accurate percentage.
    // Adafruit Feathers often use 400mAh or 500mAh batteries.
    if (!lc.setPackSize(packSize)) {
        Serial.println(F("ERROR: Failed to set battery pack size."));
        is_initialized = false;
        return false;
    }

    Serial.println(F("Battery pack size set."));
    is_initialized = true;
    return true;
}

float BatteryManager::getVoltage() {
    if (!is_initialized) {
        return 0.0;
    }
    // Read from the chip
    float voltage = lc.cellVoltage();
    // Simple check for invalid readings
    if (voltage < 2.0) {
        return 0.0;
    }
    return voltage;
}

float BatteryManager::getPercentage() {
    if (!is_initialized) {
        return 0.0;
    }
    // Read from the chip
    float percent = lc.cellPercent();
    if (percent < 0.0) {
        return 0.0;
    }
    if (percent > 100.0) {
        return 100.0;
    }
    return percent;
}

float BatteryManager::getTemperature() {
    if (!is_initialized) {
        return 0.0;
    }
    // FIX: Changed 'getTemperature()' to 'getCellTemperature()'
    return lc.getCellTemperature();
}

