#ifndef MAX30102_H
#define MAX30102_H

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"

#define BUFFER_LENGTH       100
#define SMOOTHING_WINDOW    10
#define WARMUP_TIME_MS      10000
#define SAMPLE_RATE         100

class MAX30102Sensor {
public:
  MAX30102Sensor();

  bool begin();
  void startMeasurement();
  void stopMeasurement();
  void update();

  // NEW: Returns true if new data is available (doesn't clear flag)
  bool hasNewData();
  
  // NEW: Manually clear the new data flag after reading
  void clearNewDataFlag();

  int32_t getHeartRate();
  int32_t getSpO2();
  float   getSignalQuality();
  float   getRValue();

  bool   isFingerDetected();
  bool   isWarming();
  bool   isCalibrated();
  bool   isValidReading();
  String getStatus();

private:
  MAX30105 sensor;

  bool initialized;
  bool measuring;
  bool fingerDetected;
  bool bufferFull;
  bool calibrated;
  bool newDataAvailable;

  int32_t heartRate;
  int32_t spo2;
  float   signalQuality;
  float   rValue;

  unsigned long warmupStart;
  unsigned long lastSample;
  unsigned long lastMeasurement;

  uint32_t irBuffer[BUFFER_LENGTH];
  uint32_t redBuffer[BUFFER_LENGTH];

  float hrBuffer[SMOOTHING_WINDOW];
  float spo2Buffer[SMOOTHING_WINDOW];

  int bufferIndex;
  int hrIndex;
  int spo2Index;

  uint8_t ledBrightness;

  int fingerDetectCount;
  int fingerLossCount;
  int calibrationCount;

private:
  bool  checkFingerPresence();
  void  collectSample();
  void  performMeasurement();

  void  shutdown();
  void  wakeup();
  void  adjustLEDBrightness();

  float calculateRValue();
  float calculateSignalQuality();
  int32_t calculateSpO2FromR(float rValue, float quality);

  float movingAverage(float *arr, float newValue, int *index);
  bool  isOutlier(float value, float *buffer, int index);
};

#endif