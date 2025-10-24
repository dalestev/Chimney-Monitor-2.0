/*
 * ShtManager.h
 * A simple wrapper library for the SHT30/SHT31 sensor.
 */

#ifndef SHT_MANAGER_H
#define SHT_MANAGER_H

#include <Arduino.h>
#include <Adafruit_SHT31.h> // This library works for both SHT30 and SHT31
#include <Wire.h>           // We still need main.cpp to call Wire.begin()

class ShtManager {
public:
    ShtManager(); // Constructor

    /**
     * @brief Initializes the SHT30 sensor.
     * @param i2c_addr The I2C address of the sensor (default is 0x44).
     * @return true if the chip was found and initialized, false otherwise.
     */
    // FIX: Removed the TwoWire* parameter as the Adafruit_SHT31 library
    // does not support it and only uses the global 'Wire' object.
    bool begin(uint8_t i2c_addr = SHT31_DEFAULT_ADDR);

    /**
     * @brief Gets the current temperature from the SHT30.
     * @return Temperature in Fahrenheit (F). Returns NAN on error.
     */
    float getTemperature();

    /**
     * @brief Gets the current relative humidity from the SHT30.
     * @return Relative humidity (0.0% - 100.0%). Returns NAN on error.
     */
    float getHumidity();

private:
    Adafruit_SHT31 sht;   // An instance of the Adafruit library
    bool is_initialized; // Flag to track if begin() was successful
};

#endif // SHT_MANAGER_H

