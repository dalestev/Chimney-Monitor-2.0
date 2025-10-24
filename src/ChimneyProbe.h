/*
 * ChimneyProbe.h
 * Library for managing the MAX6675 thermocouple amplifier.
 * Pin definitions are now handled internally.
 */

#ifndef CHIMNEY_PROBE_H
#define CHIMNEY_PROBE_H

#include <Arduino.h>
#include "max6675.h" // From Adafruit MAX6675 Library

// --- Pin Definitions for MAX6675 Chimney Probe ---
// (Moved from main.cpp as requested)
//
// Feather SCK pin
#define CHIMNEY_SCK_PIN 5
// Feather MISO (MI) pin
#define CHIMNEY_SO_PIN 21
// Feather A5 (GPIO 4)
#define CHIMNEY_CS_PIN 4


class ChimneyProbe {
public:
    /**
     * @brief Constructor for the Chimney Probe manager.
     * Pins are now defined in this header file.
     */
    ChimneyProbe();

    /**
     * @brief Reads the temperature from the thermocouple.
     * @return The temperature in degrees Fahrenheit, or NAN on error (e.g., probe disconnected).
     */
    float getTemperature();

private:
    // The instance of the library
    MAX6675 max6675;
};

#endif // CHIMNEY_PROBE_H

