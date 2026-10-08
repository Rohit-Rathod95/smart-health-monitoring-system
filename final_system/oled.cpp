#include "oled.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS   0x3C

// Single global display instance
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

OLEDDisplay::OLEDDisplay() {}

bool OLEDDisplay::begin() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("❌ SSD1306 allocation / init failed");
    return false;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.display();
  return true;
}

void OLEDDisplay::showSplash() {
  display.clearDisplay();

  printCentered("HealthMonitor", 4, 2);
  printCentered("System",        24, 2);
  printCentered("Starting...",   48, 1);

  display.display();
}

void OLEDDisplay::showMessage(const String &title, const String &line1, const String &line2) {
  display.clearDisplay();

  // Title bar
  drawHeader(title);

  display.setTextSize(1);
  int y = 18;

  // Wrap line1 if too long
  drawWrappedText(line1, 0, y, 21, 2);  // at most 2 lines for line1
  y += 20;

  // Wrap line2 if too long
  drawWrappedText(line2, 0, y, 21, 2);

  display.display();
}

void OLEDDisplay::showStatus(const String &status) {
  display.clearDisplay();

  // Small hint: this is often used for short status like "Connecting WiFi..."
  // but we wrap anyway so long messages don't go off-screen.
  printCentered(status, 24, 2);

  display.display();
}

void OLEDDisplay::showLiveData(int hr, int spo2, float temp) {
  display.clearDisplay();

  // Header
  drawHeader("MEASURING");

  display.setTextSize(1);

  // HR section
  display.setCursor(0, 16);
  display.print("HR:");

  display.setTextSize(2);
  display.setCursor(24, 12);
  display.print(hr > 0 ? hr : 0);

  display.setTextSize(1);
  display.setCursor(80, 18);
  display.print("BPM");

  // SpO2 section
  display.setCursor(0, 32);
  display.print("SpO2:");

  display.setTextSize(2);
  display.setCursor(40, 28);
  display.print(spo2 > 0 ? spo2 : 0);

  display.setTextSize(1);
  display.setCursor(94, 34);
  display.print("%");

  // Temp
  display.setCursor(0, 50);
  display.print("Temp:");
  display.setCursor(40, 50);
  display.printf("%.1f C", temp);

  display.display();
}

void OLEDDisplay::showFeedback(const String &feedback, const String &timestamp) {
  display.clearDisplay();

  drawHeader("Doctor Feedback");

  display.setTextSize(1);

  // Multi-line wrapped feedback text
  // 21 characters per line, up to 3–4 lines
  drawWrappedText(feedback, 0, 14, 21, 4);

  // Timestamp/subtitle in bottom area
  display.setCursor(0, 56);
  display.print(timestamp);

  display.display();
}

void OLEDDisplay::showWarmupProgress(int remainingSec) {
  if (remainingSec < 0)  remainingSec = 0;
  if (remainingSec > 99) remainingSec = 99;

  display.clearDisplay();

  printCentered("WARMUP", 4, 2);

  display.setTextSize(1);
  display.setCursor(10, 28);
  display.printf("Time: %ds remain", remainingSec);

  // Progress bar (we consider 10s total; clamp in case)
  int totalWidth = SCREEN_WIDTH - 20;
  int progress   = map(remainingSec, 10, 0, 0, totalWidth);
  if (progress < 0)          progress = 0;
  if (progress > totalWidth) progress = totalWidth;

  display.drawRect(10, 46, totalWidth, 10, SSD1306_WHITE);
  display.fillRect(10, 46, progress, 10, SSD1306_WHITE);

  display.display();
}

void OLEDDisplay::showTempProgress(float temp, int elapsedSec) {
  if (elapsedSec < 0) elapsedSec = 0;

  display.clearDisplay();

  printCentered("TEMP CHECK", 0, 2);
  display.drawLine(0, 18, SCREEN_WIDTH, 18, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 22);
  display.printf("Time: %ds", elapsedSec);

  display.setCursor(0, 34);
  display.print("Current:");

  display.setTextSize(2);
  display.setCursor(60, 38);
  display.printf("%.1f", temp);

  display.setTextSize(1);
  display.setCursor(110, 42);
  display.print("C");

  display.display();
}

void OLEDDisplay::showFinalResults(int avgHR, int avgSpO2, float avgTemp) {
  display.clearDisplay();

  drawHeader("FINAL RESULTS");

  display.setTextSize(1);
  display.setCursor(2, 14);
  display.printf("HR: %d BPM", avgHR);

  display.setCursor(2, 28);
  display.printf("SpO2: %d%%", avgSpO2);

  display.setCursor(2, 42);
  display.printf("Temp: %.1fC", avgTemp);

  display.setCursor(2, 54);
  display.print("*=Menu  #=Retry");

  display.display();
}

void OLEDDisplay::clear() {
  display.clearDisplay();
  display.display();
}

// ---------------- PRIVATE HELPERS ----------------

void OLEDDisplay::printCentered(const String &text, int y, int size) {
  display.setTextSize(size);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int x = (SCREEN_WIDTH - (int)w) / 2;
  if (x < 0) x = 0;

  display.setCursor(x, y);
  display.print(text);
}

void OLEDDisplay::drawHeader(const String &title) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.fillRect(0, 0, SCREEN_WIDTH, 10, SSD1306_BLACK); // clear header area

  // Optional: draw underline/header line
  printCentered(title, 0, 1);
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
}

void OLEDDisplay::drawWrappedText(const String &text, int x, int y,
                                  int maxCharsPerLine, int maxLines) {
  display.setTextSize(1);
  int len = text.length();
  int pos = 0;
  int line = 0;

  while (pos < len && line < maxLines) {
    int remain = len - pos;
    int take   = remain > maxCharsPerLine ? maxCharsPerLine : remain;
    String chunk = text.substring(pos, pos + take);

    display.setCursor(x, y + line * 10);
    display.print(chunk);
    pos  += take;
    line += 1;
  }
}
