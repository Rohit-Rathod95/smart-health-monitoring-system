#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class TempSensor {
public:
    TempSensor(uint8_t oneWirePin);

    // Initialize DS18B20; returns false if no sensor is found
    bool begin();

    // Non-blocking update; call this frequently from loop()
    void loop();

    // Get temperature in Celsius
    // If no valid reading yet, returns 0.0 (check available())
    float getTemperatureC();

    // Get temperature in Fahrenheit
    // If no valid reading yet, returns 0.0 (check available())
    float getTemperatureF();

    // True if we have at least one valid reading and sensor is connected
    bool available();

    // Check if temperature is in normal body temperature range (95°F - 101°F)
    bool isNormalBodyTemp();

    // Get human-readable status of body temperature
    String getBodyTempStatus();

    // Static conversion utilities
    static float celsiusToFahrenheit(float celsius);
    static float fahrenheitToCelsius(float fahrenheit);

private:
    OneWire*          oneWire;
    DallasTemperature* sensors;
    DeviceAddress     deviceAddress;

    uint8_t  pin;
    float    lastTemp;  // Stored in Celsius internally
    unsigned long lastReadMs;
    unsigned long intervalMs;          // time between conversions (ms)

    bool     sensorFound;
    bool     conversionInProgress;
    unsigned long conversionStartMs;
    unsigned long conversionDelayMs;   // depends on DS18B20 resolution
};

#endif