#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include "oled.h"
#include "keypad.h"

enum MenuAction {
  NONE,
  START_HR_SPO2,
  START_TEMP,
  SEND_CSV,
  VIEW_FEEDBACK,
  AUDIO_MENU,
  SETTINGS
};

class MenuManager {
public:
  MenuManager(OLEDDisplay *oled, KeypadManager *keypad);
  void begin();
  MenuAction loop();
  int getSelectedUserId();
  void setSelectedUserId(int id);   // NEW

private:
  OLEDDisplay   *oled;
  KeypadManager *keypad;

  bool          menuVisible;
  unsigned long lastInteractionMs;
  unsigned long autoRedrawIntervalMs;

  int           selectedUserId;     // NEW

  void drawMenu();
};

#endif
