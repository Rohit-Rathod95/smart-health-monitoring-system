#include "wifi_manager.h"

WiFiManagerESP::WiFiManagerESP() {}

void WiFiManagerESP::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
}

bool WiFiManagerESP::connect(const String &ssid, const String &pass, int timeoutMs) {
  if (ssid.length() == 0) return false;

  if (pass.length() == 0) {
    WiFi.begin(ssid.c_str());
  } else {
    WiFi.begin(ssid.c_str(), pass.c_str());
  }

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < (unsigned long)timeoutMs) {
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool WiFiManagerESP::scanNetworks() {
  int n = WiFi.scanNetworks();
  return n > 0;
}

bool WiFiManagerESP::interactiveConnect(OLEDDisplay* oled, KeypadManager* keypad, int timeoutMs) {
  if (!oled || !keypad) return false;

  oled->showStatus("Scanning WiFi...");
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    oled->showMessage("WiFi", "No networks", "found");
    delay(1500);
    return false;
  }

  int index = 0;

  while (true) {
    // Clamp index
    if (index < 0)       index = n - 1;
    if (index >= n)      index = 0;

    String ssid = WiFi.SSID(index);
    int    rssi = WiFi.RSSI(index);
    wifi_auth_mode_t enc = WiFi.encryptionType(index);

    ssid.trim();
    if (ssid.length() == 0) ssid = "<hidden>";

    String title = "WiFi (" + String(index + 1) + "/" + String(n) + ")";

    String line1 = ssid.substring(0, 21); // OLED line width
    String lockStr = (enc == WIFI_AUTH_OPEN) ? "OPEN" : "LOCK";
    String line2 = "A/B sel  #=OK " + lockStr;

    oled->showMessage(title, line1, line2);

    // Wait for a navigation/selection key
    while (true) {
      char key = keypad->getKey();
      if (key == 'A') {      // previous network
        index--;
        break;
      }
      if (key == 'B') {      // next network
        index++;
        break;
      }
      if (key == 'D') {      // cancel
        oled->showMessage("WiFi", "Cancelled", "");
        delay(1000);
        return false;
      }
      if (key == '#') {      // select this network
        bool isOpen = (enc == WIFI_AUTH_OPEN);
        String password = "";

        if (!isOpen) {
          oled->showMessage("WiFi", "Enter password", "#=OK *=Bksp");
          password = keypad->inputText(32, '#');  // blocking text input

          if (password.length() == 0) {
            oled->showMessage("WiFi", "Cancelled", "");
            delay(1000);
            return false;
          }
        }

        oled->showStatus("Connecting...");
        bool ok = connect(ssid, isOpen ? "" : password, timeoutMs);

        if (ok) {
          String ip = WiFi.localIP().toString();
          oled->showMessage("WiFi", "Connected", ip);
          delay(1500);
          return true;
        } else {
          oled->showMessage("WiFi", "Failed", "Try again");
          delay(1500);
          // stay in outer while, let user pick again
          break;
        }
      }

      delay(50); // small poll delay
    }
  }
}

String WiFiManagerESP::getLocalIP() {
  return WiFi.localIP().toString();
}

bool WiFiManagerESP::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}
