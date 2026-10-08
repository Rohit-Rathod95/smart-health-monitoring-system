#ifndef KEYPAD_H
#define KEYPAD_H

#include <Arduino.h>

class KeypadManager {
public:
  KeypadManager(uint8_t rowPins[4], uint8_t colPins[4]);

  void begin();

  // Debounced, non-blocking key read.
  // Returns a single key character ONCE per press,
  // or 0 if no new key press is detected.
  char getKey();

  // Blocking text input (used only where you really want to wait)
  // '#': default "enter" key, '*' = backspace
  String inputText(int maxLen, char endChar = '#');

private:
  uint8_t rPins[4];
  uint8_t cPins[4];

  // Internal state for debouncing
  char lastRawKey;
  char lastReturnedKey;
  unsigned long lastChangeMs;
  uint16_t debounceMs;

  // Scan matrix once and return raw key (no debounce):
  // pressed key char or 0 if none.
  char scanOnce();
};

#endif
