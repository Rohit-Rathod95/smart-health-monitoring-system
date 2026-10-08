#include "menu.h"

MenuManager::MenuManager(OLEDDisplay *o, KeypadManager *k)
  : oled(o),
    keypad(k),
    menuVisible(false),
    lastInteractionMs(0),
    autoRedrawIntervalMs(8000),
    selectedUserId(1)   // default user ID
{
}

void MenuManager::begin() {
  drawMenu();
}

void MenuManager::drawMenu() {
  if (!oled) return;

  // Show current user at bottom as U:<id>
  String line1 = "A:HR B:Temp C:CSV";
  String line2 = "D:Feed1:Aud2:Set U:" + String(selectedUserId);

  oled->showMessage("Main Menu", line1, line2);

  menuVisible       = true;
  lastInteractionMs = millis();
}

MenuAction MenuManager::loop() {
  char key = keypad->getKey();
  unsigned long now = millis();

  if (!key) {
    if (!menuVisible && (now - lastInteractionMs > autoRedrawIntervalMs)) {
      drawMenu();
    }
    return NONE;
  }

  lastInteractionMs = now;
  menuVisible       = true;

  switch (key) {
    case 'A': return START_HR_SPO2;
    case 'B': return START_TEMP;
    case 'C': return SEND_CSV;
    case 'D': return VIEW_FEEDBACK;
    case '1': return AUDIO_MENU;
    case '2': return SETTINGS;
    default:  return NONE;
  }
}

int MenuManager::getSelectedUserId() {
  return selectedUserId;
}

void MenuManager::setSelectedUserId(int id) {
  selectedUserId = id;
  drawMenu();   // refresh menu to reflect new user
}
