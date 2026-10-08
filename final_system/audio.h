#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>

class AudioManager {
public:
  AudioManager(int rxPin = -1, int txPin = -1);

  // Initialize the UART for the DFPlayer / audio module.
  void begin();

  // Simple event-based sounds
  void playWelcome();
  void playStartMeasurement();
  void playEndMeasurement();
  void playError();
  void playFeedbackNotification();

private:
  int rx, tx;
  bool initialized;
  unsigned long lastCommandMs;
  HardwareSerial* playerSerial;

  // Minimal DFPlayer-style command helper
  void sendCommand(uint8_t cmd, uint8_t paramHigh, uint8_t paramLow);
};

#endif
