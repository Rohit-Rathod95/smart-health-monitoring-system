#include "audio.h"
#include <HardwareSerial.h>

// Use UART1 (Serial1) for DFPlayer or UART audio module
static HardwareSerial MySerial(1);

AudioManager::AudioManager(int rxPin, int txPin)
  : rx(rxPin),
    tx(txPin),
    initialized(false),
    lastCommandMs(0),
    playerSerial(&MySerial) {}

void AudioManager::begin() {
  if (rx < 0 || tx < 0) {
    Serial.println("⚠ AudioManager: RX/TX pins not configured, audio disabled");
    initialized = false;
    return;
  }

  // Start UART1 for audio module
  playerSerial->begin(9600, SERIAL_8N1, rx, tx);
  delay(50); // small settle time

  initialized = true;
  Serial.printf("✅ AudioManager: DFPlayer UART started on RX=%d TX=%d\n", rx, tx);

  // Optional: you could send an "init volume" command here if your module supports it
  // e.g. sendCommand(0x06, 0x00, 20); // set volume to 20
}

void AudioManager::sendCommand(uint8_t cmd, uint8_t paramHigh, uint8_t paramLow) {
  if (!initialized) return;

  // Throttle commands a bit to avoid overrunning the DFPlayer on rapid events
  unsigned long now = millis();
  if (now - lastCommandMs < 80) {
    return;
  }
  lastCommandMs = now;

  uint8_t packet[] = {
    0x7E,       // start byte
    0xFF,       // version
    0x06,       // length
    cmd,        // command
    0x00,       // no feedback
    paramHigh,  // parameter high byte
    paramLow,   // parameter low byte
    0xEF        // end byte
  };

  playerSerial->write(packet, sizeof(packet));
}

void AudioManager::playWelcome() {
  // Plays track 1
  sendCommand(0x03, 0x00, 0x01);
}

void AudioManager::playStartMeasurement() {
  // Plays track 2
  sendCommand(0x03, 0x00, 0x02);
}

void AudioManager::playEndMeasurement() {
  // Plays track 3
  sendCommand(0x04, 0x00, 0x03);
}

void AudioManager::playError() {
  // Plays track 4 (error / alert tone)
  sendCommand(0x00, 0x00, 0x04);
}

void AudioManager::playFeedbackNotification() {
  // Plays track 5 (feedback / WhatsApp notification)
  sendCommand(0x05, 0x00, 0x05);
}
