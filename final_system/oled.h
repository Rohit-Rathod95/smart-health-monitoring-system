#ifndef OLED_H
#define OLED_H

#include <Arduino.h>

class OLEDDisplay {
public:
  OLEDDisplay();

  // Initialize the SSD1306 display; returns false if init fails.
  bool begin();

  // Splash / boot screen
  void showSplash();

  // Generic message screen, three lines (title + 2 lines)
  void showMessage(const String &title, const String &line1, const String &line2);

  // Simple centered status text (e.g. "Connecting WiFi...")
  void showStatus(const String &status);

  // Main live monitoring view
  void showLiveData(int hr, int spo2, float temp);

  // Show doctor feedback text + a short subtitle/timestamp
  void showFeedback(const String &feedback, const String &timestamp);

  // Warmup progress (remainingSec is seconds left)
  void showWarmupProgress(int remainingSec);

  // Temperature measurement progress
  void showTempProgress(float temp, int elapsedSec);

  // Final summary screen for a measurement session
  void showFinalResults(int avgHR, int avgSpO2, float avgTemp);

  // Clear the display
  void clear();

private:
  // Utilities
  void printCentered(const String &text, int y, int size = 1);
  void drawHeader(const String &title);
  void drawWrappedText(const String &text, int x, int y, int maxCharsPerLine, int maxLines);
};

#endif
