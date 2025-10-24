/*
 * ChimneyProbe.cpp
 * Implementation for the MAX6675 library.
 */

#include "ChimneyProbe.h"

// The constructor now initializes the library instance
// using the pins defined in ChimneyProbe.h
// The Adafruit MAX6675 library constructor is (CLK, CS, DO)
ChimneyProbe::ChimneyProbe() :
    max6675(CHIMNEY_SCK_PIN, CHIMNEY_CS_PIN, CHIMNEY_SO_PIN) { 
    
    // Initialization is handled by the constructor.
    // The main .cpp file will add a 500ms delay in setup()
    // to allow the chip to stabilize before its first read.
}

/**
 * @brief Reads the temperature from the thermocouple.
 */
float ChimneyProbe::getTemperature() {
    // readFahrenheit() handles all the SPI communication
    float tempF = max6675.readFahrenheit();

    // The library returns NAN (Not a Number) if the probe
    // is disconnected or has a fault.
    if (isnan(tempF)) {
        Serial.println("Warning: Chimney probe read error (is it disconnected?)");
        // We'll still return NAN to the main loop to handle
        return NAN; 
    }

    return tempF;
}

