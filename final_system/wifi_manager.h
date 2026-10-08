#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "oled.h"
#include "keypad.h"

class WiFiManagerESP {
public:
  WiFiManagerESP();

  // Basic init: station mode, disconnect etc.
  void begin();

  // Direct connect with given SSID & password (kept for compatibility)
  bool connect(const String &ssid, const String &pass, int timeoutMs = 15000);

  // Simple scan (returns true if any networks found)
  bool scanNetworks();

  // New: interactive connect using OLED + Keypad
  // - Scans Wi-Fi
  // - Lets user scroll networks with A/B
  // - Select with #
  // - D to cancel
  // - For secured networks, asks password via keypad
  bool interactiveConnect(OLEDDisplay* oled, KeypadManager* keypad, int timeoutMs = 15000);

  String getLocalIP();
  bool   isConnected();
};

#endif
