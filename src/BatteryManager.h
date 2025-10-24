/*
 * BatteryManager.h
 * A simple wrapper library for the LC709203F battery monitoring chip.
 */

#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include <Adafruit_LC709203F.h> // Include the Adafruit library
#include <Wire.h>                // We need this for I2C

class BatteryManager {
public:
    BatteryManager(); // Constructor

    /**
     * @brief Initializes the battery monitor chip.
     * @param wireInstance A pointer to the I2C bus (default is &Wire).
     * @param packSize The size of your battery pack (e.g., LC709203F_APA_500MAH).
     * @return true if the chip was found and initialized, false otherwise.
     */
    // FIX: Changed 'lc709203f_packsize_t' to the correct type 'lc709203_adjustment_t'
    bool begin(TwoWire *wireInstance = &Wire, lc709203_adjustment_t packSize = LC709203F_APA_500MAH);

    /**
     * @brief Gets the current battery voltage.
     * @return Battery voltage in Volts (V).
     */
    float getVoltage();

    /**
     * @brief Gets the current battery percentage.
     * @return Battery percentage (0.0% - 100.0%).
     */
    float getPercentage();

    /**
     * @brief Gets the temperature from the chip's internal sensor.
     * @return Temperature in Celsius (C).
     */
    float getTemperature();

private:
    Adafruit_LC709203F lc; // An instance of the Adafruit library
    bool is_initialized;   // Flag to track if begin() was successful
};

#endif // BATTERY_MANAGER_H

