#include "temperature.h"
#include <OneWire.h>
#include <DallasTemperature.h>

TempSensor::TempSensor(uint8_t oneWirePin)
  : oneWire(nullptr),
    sensors(nullptr),
    pin(oneWirePin),
    lastTemp(NAN),
    lastReadMs(0),
    intervalMs(2000),          // new reading every 2 seconds
    sensorFound(false),
    conversionInProgress(false),
    conversionStartMs(0),
    conversionDelayMs(750)     // 12-bit resolution → ~750 ms
{
}

bool TempSensor::begin() {
  oneWire = new OneWire(pin);
  sensors = new DallasTemperature(oneWire);
  sensors->begin();
  delay(10);

  if (!sensors->getAddress(deviceAddress, 0)) {
    Serial.println("❌ DS18B20 not found on OneWire bus");
    sensorFound = false;
    return false;
  }

  sensorFound = true;

  // Configure sensor
  sensors->setResolution(deviceAddress, 12);         // max resolution
  sensors->setWaitForConversion(false);              // non-blocking mode
  conversionDelayMs = 750;                           // typical for 12-bit

  lastTemp              = NAN;
  lastReadMs            = 0;
  conversionInProgress  = false;

  Serial.println("✅ DS18B20 temperature sensor initialized");
  return true;
}

void TempSensor::loop() {
  if (!sensorFound || sensors == nullptr) return;

  unsigned long now = millis();

  // If no conversion is in progress, start a new one at the desired interval
  if (!conversionInProgress) {
    if (now - lastReadMs >= intervalMs) {
      sensors->requestTemperaturesByAddress(deviceAddress);
      conversionStartMs     = now;
      conversionInProgress  = true;
    }
  } else {
    // Wait for conversion time before reading
    if (now - conversionStartMs >= conversionDelayMs) {
      float t = sensors->getTempC(deviceAddress);
      if (t != DEVICE_DISCONNECTED_C) {
        // simple exponential smoothing: 75% old + 25% new
        if (isnan(lastTemp)) {
          lastTemp = t;
        } else {
          lastTemp = (lastTemp * 3.0f + t) / 4.0f;
        }
        
        // Debug output in both units
        Serial.printf("🌡 Temperature: %.1f°C / %.1f°F\n", lastTemp, celsiusToFahrenheit(lastTemp));
      } else {
        Serial.println("⚠ DS18B20 read error (disconnected)");
      }

      lastReadMs           = now;
      conversionInProgress = false;
    }
  }
}

float TempSensor::celsiusToFahrenheit(float celsius) {
  return (celsius * 9.0f / 5.0f) + 32.0f;
}

float TempSensor::fahrenheitToCelsius(float fahrenheit) {
  return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}

float TempSensor::getTemperatureC() {
  // Return 0.0 if no valid reading yet
  if (!available()) {
    return 0.0f;
  }
  return lastTemp;
}

float TempSensor::getTemperatureF() {
  // Return 0.0 if no valid reading yet
  if (!available()) {
    return 0.0f;
  }
  return celsiusToFahrenheit(lastTemp);
}

bool TempSensor::available() {
  return sensorFound && !isnan(lastTemp) &&
         lastTemp > -50.0f && lastTemp < 150.0f;
}

bool TempSensor::isNormalBodyTemp() {
  if (!available()) return false;
  
  float tempF = celsiusToFahrenheit(lastTemp);
  
  // Normal body temperature range: 95°F to 101°F (97°F ± 2°F to 99°F ± 2°F)
  return (tempF >= 95.0f && tempF <= 101.0f);
}

String TempSensor::getBodyTempStatus() {
  if (!available()) {
    return "No reading";
  }
  
  float tempF = celsiusToFahrenheit(lastTemp);
  
  if (tempF < 95.0f) {
    return "LOW (Hypothermia)";
  } else if (tempF <= 97.0f) {
    return "Below Normal";
  } else if (tempF <= 99.0f) {
    return "Normal";
  } else if (tempF <= 101.0f) {
    return "Elevated";
  } else {
    return "HIGH (Fever)";
  }
}