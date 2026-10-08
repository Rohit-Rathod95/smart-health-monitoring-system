#include "keypad.h"
#include <Arduino.h>

// Simple matrix keypad implementation with basic debouncing.
// Returns one key event per press; non-blocking for the main loop.

KeypadManager::KeypadManager(uint8_t rowPins[4], uint8_t colPins[4])
  : lastRawKey(0),
    lastReturnedKey(0),
    lastChangeMs(0),
    debounceMs(40) // ~40ms debounce
{
  for (int i = 0; i < 4; i++) {
    rPins[i] = rowPins[i];
    cPins[i] = colPins[i];
  }
}

void KeypadManager::begin() {
  for (int i = 0; i < 4; i++) {
    pinMode(rPins[i], INPUT_PULLUP);
  }
  for (int i = 0; i < 4; i++) {
    pinMode(cPins[i], OUTPUT);
    digitalWrite(cPins[i], HIGH); // idle HIGH
  }
}

char KeypadManager::scanOnce() {
  // Key layout:
  // [ [1,2,3,A],
  //   [4,5,6,B],
  //   [7,8,9,C],
  //   [*,0,#,D] ]
  static const char map[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
  };

  for (int c = 0; c < 4; c++) {
    digitalWrite(cPins[c], LOW);
    delayMicroseconds(3); // small settle time

    for (int r = 0; r < 4; r++) {
      if (digitalRead(rPins[r]) == LOW) {
        // Found a pressed key
        digitalWrite(cPins[c], HIGH);
        return map[r][c];
      }
    }

    digitalWrite(cPins[c], HIGH);
  }

  return 0;
}

char KeypadManager::getKey() {
  unsigned long now = millis();

  // Raw read from matrix
  char rawKey = scanOnce();

  // If raw state changed, reset debounce timer
  if (rawKey != lastRawKey) {
    lastRawKey   = rawKey;
    lastChangeMs = now;
  }

  // If change is recent, still bouncing → don’t trust yet
  if (now - lastChangeMs < debounceMs) {
    return 0;
  }

  // Stable state after debounce:
  //  - If a key is pressed AND we haven't already reported it, report once.
  //  - If no key is pressed, clear lastReturnedKey so next press will be reported again.
  if (rawKey != 0 && lastReturnedKey == 0) {
    lastReturnedKey = rawKey;
    return rawKey;        // one event per press
  }

  if (rawKey == 0) {
    // Key released; allow next press to be reported
    lastReturnedKey = 0;
  }

  return 0;
}

String KeypadManager::inputText(int maxLen, char endChar) {
  // Blocking helper for places where you intentionally want to wait
  // for text (e.g. password entry in settings).
  String s;

  while (true) {
    char k = getKey();
    if (k) {
      if (k == endChar) {
        return s;
      }
      if (k == '*') { // backspace
        if (s.length() > 0) {
          s.remove(s.length() - 1);
        }
      } else {
        if ((int)s.length() < maxLen) {
          s += k;
        }
      }
    }
    delay(30); // keep checks responsive but not too busy
  }
}
