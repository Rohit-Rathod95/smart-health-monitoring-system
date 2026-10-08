// final_system.ino - Health Monitoring System with State Machine
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// Include all module headers
#include "oled.h"
#include "menu.h"
#include "keypad.h"
#include "audio.h"
#include "max30102.h"
#include "temperature.h"
#include "sdmanager.h"
#include "users.h"
#include "wifi_manager.h"
#include "twilio_sender.h"
#include "twilio_feedback.h"

#include <WebServer.h>
#include <WiFi.h>

// ---------------- CONFIG ----------------
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

const String TWILIO_SID = "YOUR_TWILIO_SID";
const String TWILIO_TOKEN = "YOUR_TWILIO_TOKEN";
const String TWILIO_WHATSAPP_FROM = "YOUR_TWILIO_WHATSAPP_NUMBER";
const String DOCTOR_PHONE = "YOUR_DOCTOR_PHONE";

#define SD_CS_PIN    5
#define ONEWIRE_PIN  0
#define DFPLAYER_RX  16
#define DFPLAYER_TX  17

uint8_t rowPins[4] = {25, 26, 33, 32};
uint8_t colPins[4] = {13, 12, 14, 27};

// ---------------- TIMING CONSTANTS ----------------
const unsigned long WARMUP_DURATION_MS      = 10000UL;
const unsigned long MEASURE_DURATION_MS     = 60000UL;
const unsigned long TEMP_DURATION_MS        = 60000UL;
const unsigned long FEEDBACK_POLL_INTERVAL  = 15000UL;
const unsigned long LOOP_BASE_DELAY_MS      = 10UL;

// ---------------- GLOBAL OBJECTS ----------------
OLEDDisplay      oled;
KeypadManager    keypad(rowPins, colPins);
MenuManager     *menu = nullptr;
AudioManager     audio(DFPLAYER_RX, DFPLAYER_TX);
SDManager        sd(SD_CS_PIN);
Users           *users = nullptr;
MAX30102Sensor   maxSensor;
TempSensor       tempSensor(ONEWIRE_PIN);
WiFiManagerESP   wifiManager;
TwilioSender    *twilioSender   = nullptr;
TwilioFeedback  *twilioFeedback = nullptr;

WebServer        server(80);

// ---------------- STATE MANAGEMENT ----------------
enum SystemState {
  STATE_IDLE,
  STATE_WARMUP,
  STATE_MEASURING_HR,
  STATE_MEASURING_TEMP,
  STATE_CONTINUOUS_MONITOR,
  STATE_COMPLETE
};

SystemState   currentState   = STATE_IDLE;
unsigned long stateStartTime = 0;

// ---------------- MEASUREMENT SESSION ----------------
struct MeasurementSession {
  long  hrSum      = 0;
  long  spo2Sum    = 0;
  float tempSum    = 0;
  int   hrCount    = 0;
  int   spo2Count  = 0;
  int   tempCount  = 0;
  unsigned long startTime   = 0;
  int   cycleNumber = 0;

  void reset() {
    hrSum      = 0;
    spo2Sum    = 0;
    tempSum    = 0;
    hrCount    = 0;
    spo2Count  = 0;
    tempCount  = 0;
    startTime  = millis();
    cycleNumber = 0;
  }

  int   getAvgHR()    { return (hrCount    > 0) ? hrSum   / hrCount   : 0; }
  int   getAvgSpO2()  { return (spo2Count  > 0) ? spo2Sum / spo2Count : 0; }
  float getAvgTemp()  { return (tempCount  > 0) ? tempSum / tempCount : 0.0f; }
} session;

// ---------------- FEEDBACK POLLING ----------------
unsigned long lastFeedbackPoll = 0;

// ---------------- THRESHOLD CHECKING ----------------
struct SafeRange {
  int   hr_low;
  int   hr_high;
  int   spo2_low;
  float temp_low;
  float temp_high;
};

SafeRange getThresholds(int age, const String& gender) {
  SafeRange range;

  if (age >= 13 && age <= 19) {
    range.hr_low = 60; range.hr_high = 100;
  } else if (age >= 20 && age <= 55) {
    range.hr_low = 60; range.hr_high = 100;
  } else if (age >= 56 && age <= 70) {
    range.hr_low = 65; range.hr_high = 100;
  } else {
    range.hr_low = 65; range.hr_high = 95;
  }

  range.spo2_low  = 95;
  range.temp_low  = 95.0f;
  range.temp_high = 101.0f;

  return range;
}

bool checkAbnormal(int hr, int spo2, float tempf, int uid) {
  if (!users) return false;
  UserProfile* profile = users->getUserProfileById(uid);
  if (!profile) return false;

  SafeRange range = getThresholds(profile->age, profile->gender);

  if (hr   > 0 && (hr   < range.hr_low   || hr   > range.hr_high)) return true;
  if (spo2 > 0 &&  spo2 < range.spo2_low)                          return true;
  if (tempf > 0 && (tempf < range.temp_low || tempf > range.temp_high)) return true;

  return false;
}

String getAlertReason(int hr, int spo2, float tempf, int uid) {
  if (!users) return "Unknown user";
  UserProfile* profile = users->getUserProfileById(uid);
  if (!profile) return "Unknown user";

  SafeRange range = getThresholds(profile->age, profile->gender);
  String reason;

  if (hr > 0 && hr < range.hr_low) {
    reason += "⚠ LOW HR: " + String(hr) + " BPM (Normal: " +
              String(range.hr_low) + "-" + String(range.hr_high) + ")\n";
  } else if (hr > 0 && hr > range.hr_high) {
    reason += "⚠ HIGH HR: " + String(hr) + " BPM (Normal: " +
              String(range.hr_low) + "-" + String(range.hr_high) + ")\n";
  }

  if (spo2 > 0 && spo2 < range.spo2_low) {
    reason += "⚠ LOW SpO2: " + String(spo2) + "% (Normal: >" +
              String(range.spo2_low) + "%)\n";
  }

  if (tempf > 0 && tempf < range.temp_low) {
    reason += "⚠ LOW TEMP: " + String(tempf, 1) + "°F (Normal: " +
              String(range.temp_low, 1) + "-" + String(range.temp_high, 1) + ")\n";
  } else if (tempf > 0 && tempf > range.temp_high) {
    reason += "⚠ HIGH TEMP: " + String(tempf, 1) + "°F (Normal: " +
              String(range.temp_low, 1) + "-" + String(range.temp_high, 1) + ")\n";
  }

  if (reason.length() == 0) {
    reason = "Values slightly outside normal range.";
  }

  return reason;
}

void sendAlertToDoctor(int uid, int hr, int spo2, float tempf) {
  if (!users || !twilioSender) return;
  UserProfile* profile = users->getUserProfileById(uid);
  if (!profile) return;

  String alert = "🚨 HEALTH ALERT\n";
  alert += "Patient: " + profile->name + "\n";
  alert += "Age: " + String(profile->age) + ", " + profile->gender + "\n\n";
  alert += getAlertReason(hr, spo2, tempf, uid);
  alert += "\nCurrent Readings:\n";
  alert += "HR: " + String(hr) + " BPM\n";
  alert += "SpO2: " + String(spo2) + "%\n";
  alert += "Temp: " + String(tempf, 1) + "°F";

  bool sent = twilioSender->sendWhatsAppWithMedia(DOCTOR_PHONE, alert, "");

  if (sent) {
    Serial.println("✅ Alert sent to doctor");
    oled.showMessage("ALERT SENT!", "Doctor notified", "");
  } else {
    Serial.println("❌ Alert send failed");
    oled.showMessage("ALERT FAILED", "Check network", "");
  }

  audio.playError();
  delay(1000);
}

void sendReportToDoctor(int uid) {
  if (!twilioSender) return;

  oled.showStatus("Generating CSV...");
  String fname = sd.generateCSVReport(uid, &session);

  String mediaUrl = "http://" + wifiManager.getLocalIP() + "/report/" + fname;

  UserProfile* profile = users ? users->getUserProfileById(uid) : nullptr;

  String body = "Health Report - " +
                String(profile ? profile->name : ("User " + String(uid)));

  body += "\n\nAverage Readings:";
  body += "\nHR: "   + String(session.getAvgHR())       + " BPM";
  body += "\nSpO2: " + String(session.getAvgSpO2())     + "%";
  body += "\nTemp: " + String(session.getAvgTemp(), 1)  + "°F";
  body += "\n\nCSV Report: " + mediaUrl;

  bool ok = twilioSender->sendWhatsAppWithMedia(DOCTOR_PHONE, body, mediaUrl);

  oled.showMessage("Report", ok ? "Sent ✓" : "Failed ✗", "");
  audio.playFeedbackNotification();
  delay(1500);
}

// ---------------- TWILIO FEEDBACK HELPERS ----------------
// UPDATED FUNCTION - Uses new parsing method
String getLatestFeedbackMessage() {
  if (!twilioFeedback) return "";
  
  String json = twilioFeedback->fetchLatest();
  if (json.length() == 0) {
    Serial.println("⚠ No JSON response from Twilio");
    return "";
  }
  
  String body = twilioFeedback->parseLatestMessage(json);
  return body;
}

// ---------------- STATE HANDLERS ----------------
void handleWarmupState() {
  maxSensor.update();

  if (!maxSensor.isFingerDetected()) {
    oled.showMessage("Place finger", "on sensor", "");
    return;
  }

  unsigned long elapsed   = millis() - stateStartTime;
  unsigned long remaining = (WARMUP_DURATION_MS > elapsed)
                            ? (WARMUP_DURATION_MS - elapsed) / 1000UL
                            : 0;

  if (elapsed >= WARMUP_DURATION_MS) {
    currentState   = STATE_MEASURING_HR;
    stateStartTime = millis();
    oled.showStatus("Measuring...");
    audio.playStartMeasurement();
    Serial.println("🟢 Starting measurement phase");
  } else {
    oled.showWarmupProgress(remaining);
  }
}

void handleMeasuringState() {
  maxSensor.update();

  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 2000) {
    lastDebug = millis();
    Serial.printf("DEBUG: Finger=%d, Calibrated=%d, HasNew=%d, Quality=%.1f%%\n",
                  maxSensor.isFingerDetected(),
                  maxSensor.isCalibrated(),
                  maxSensor.hasNewData(),
                  maxSensor.getSignalQuality());
    
    if (maxSensor.isCalibrated()) {
      Serial.printf("       HR=%d, SpO2=%d%%\n",
                    maxSensor.getHeartRate(),
                    maxSensor.getSpO2());
    }
  }

  if (!maxSensor.isFingerDetected()) {
    oled.showMessage("Finger lost!", "Replace now", "");
    return;
  }

  if (maxSensor.hasNewData()) {
    int   hr    = maxSensor.getHeartRate();
    int   spo2  = maxSensor.getSpO2();
    float tempF = tempSensor.getTemperatureF();  // CHANGED: Use Fahrenheit

    // ADDED: Validate temperature reading
    if (tempF < 50.0f || tempF > 120.0f) {
      tempF = 98.6f;  // Default normal body temp in Fahrenheit
      Serial.println("⚠ Invalid temp reading, using default 98.6°F");
    }

    if (hr > 0 && hr < 200) {
      session.hrSum += hr;
      session.hrCount++;
    }
    
    if (spo2 > 0 && spo2 <= 100) {
      session.spo2Sum += spo2;
      session.spo2Count++;
    }

    // ADDED: Track temperature in session
    if (tempF > 50.0f && tempF < 120.0f) {
      session.tempSum += tempF;
      session.tempCount++;
    }

    oled.showLiveData(hr, spo2, tempF);  // CHANGED: Show °F

    int uid = menu->getSelectedUserId();
    sd.logMeasurement(uid, hr, spo2, tempF);  // CHANGED: Log in °F

    Serial.printf("📊 LOGGED: HR:%d SpO2:%d%% Temp:%.1f°F (Count: HR=%d SpO2=%d)\n", 
                  hr, spo2, tempF, session.hrCount, session.spo2Count);
    
    maxSensor.clearNewDataFlag();
  }

  char key = keypad.getKey();
  if (key == '*') {
    maxSensor.stopMeasurement();

    int   avgHR    = session.getAvgHR();
    int   avgSpO2  = session.getAvgSpO2();
    float avgTempF = session.getAvgTemp();  // Now in Fahrenheit

    Serial.println("\n═══ MEASUREMENT STOPPED ═══");
    Serial.printf("HR: %d BPM (%d samples)\n",      avgHR,    session.hrCount);
    Serial.printf("SpO2: %d%% (%d samples)\n",      avgSpO2,  session.spo2Count);
    Serial.printf("Temp: %.1f°F (%d samples)\n",    avgTempF, session.tempCount);
    Serial.println("═══════════════════════════\n");

    int uid = menu->getSelectedUserId();
    if (checkAbnormal(avgHR, avgSpO2, avgTempF, uid)) {
      Serial.println("🚨 ABNORMAL VALUES DETECTED!");
      sendAlertToDoctor(uid, avgHR, avgSpO2, avgTempF);
    }

    currentState   = STATE_MEASURING_TEMP;
    stateStartTime = millis();
    oled.showStatus("Temp check...");
    Serial.println("🌡 Starting temperature measurement");
  }
}

void handleTempState() {
  tempSensor.loop();
  float tempF = tempSensor.getTemperatureF();  // CHANGED: Use Fahrenheit

  // CHANGED: Validate Fahrenheit range
  if (tempF > 50.0f && tempF < 120.0f) {
    session.tempSum += tempF;
    session.tempCount++;
  }

  unsigned long elapsed    = millis() - stateStartTime;
  int           elapsedSec = elapsed / 1000UL;

  static unsigned long lastTempUiUpdate = 0;
  if (millis() - lastTempUiUpdate > 500) {
    lastTempUiUpdate = millis();
    oled.showTempProgress(tempF, elapsedSec);  // CHANGED: Show °F
  }

  if (elapsed >= TEMP_DURATION_MS) {
    float avgTempF = session.getAvgTemp();  // Now in Fahrenheit
    int   avgHR    = session.getAvgHR();
    int   avgSpO2  = session.getAvgSpO2();
    int   uid      = menu->getSelectedUserId();

    Serial.println("\n═══ FINAL TEMP ═══");
    Serial.printf("Avg Temp: %.1f°F (%d samples)\n", avgTempF, session.tempCount);
    Serial.println("═══════════════════\n");

    if (checkAbnormal(avgHR, avgSpO2, avgTempF, uid)) {
      Serial.println("🚨 TEMPERATURE ABNORMAL!");
      sendAlertToDoctor(uid, avgHR, avgSpO2, avgTempF);
    }

    currentState = STATE_COMPLETE;
    audio.playEndMeasurement();
    oled.showFinalResults(avgHR, avgSpO2, avgTempF);  // CHANGED: Show °F

    Serial.println("\n✅ MEASUREMENT COMPLETE");
    Serial.println("   *  → Menu");
    Serial.println("   #  → Repeat");
    Serial.println("   C  → Send report\n");
  }
}

void handleCompleteState() {
  char key = keypad.getKey();

  if (key == '*') {
    currentState = STATE_IDLE;
    session.reset();
    oled.showMessage("Ready", "Use keypad", "");
    Serial.println("🔄 Returned to menu");
  } else if (key == '#') {
    session.reset();
    currentState   = STATE_WARMUP;
    stateStartTime = millis();
    maxSensor.startMeasurement();
    oled.showStatus("Restarting...");
    Serial.println("🔄 Restarting measurement");
  } else if (key == 'C') {
    int uid = menu->getSelectedUserId();
    sendReportToDoctor(uid);
  }

  delay(50);
}

void handleContinuousMonitor() {
  char key = keypad.getKey();
  if (key == '*') {
    maxSensor.stopMeasurement();
    currentState = STATE_IDLE;
    oled.showMessage("Stopped", "Returning...", "");
    delay(500);
    return;
  }

  maxSensor.update();
  tempSensor.loop();

  if (maxSensor.hasNewData() && maxSensor.isValidReading()) {
    int   hr   = maxSensor.getHeartRate();
    int   spo2 = maxSensor.getSpO2();

    session.hrSum    += hr;
    session.spo2Sum  += spo2;
    session.hrCount++;
    session.spo2Count++;
  }

  unsigned long elapsed = millis() - stateStartTime;
  if (elapsed >= MEASURE_DURATION_MS) {
    session.cycleNumber++;

    int   avgHR    = session.getAvgHR();
    int   avgSpO2  = session.getAvgSpO2();
    float avgTempF = tempSensor.getTemperatureF();  // CHANGED: Use Fahrenheit

    int uid = menu->getSelectedUserId();
    sd.logCycle(uid, session.cycleNumber, avgHR, avgSpO2, avgTempF);

    Serial.printf("📊 Cycle #%d - HR:%d SpO2:%d%% Temp:%.1f°F\n",
                  session.cycleNumber, avgHR, avgSpO2, avgTempF);

    if (checkAbnormal(avgHR, avgSpO2, avgTempF, uid)) {
      sendAlertToDoctor(uid, avgHR, avgSpO2, avgTempF);
    }

    session.hrSum    = 0;
    session.spo2Sum  = 0;
    session.hrCount  = 0;
    session.spo2Count = 0;
    stateStartTime   = millis();
  }
}

// ---------------- WEB SERVER ----------------
void handleFileServer() {
  String uri = server.uri();

  if (uri.startsWith("/report/")) {
    String fname = "/logs/" + uri.substring(8);
    if (SD.exists(fname.c_str())) {
      File f = SD.open(fname.c_str(), FILE_READ);
      if (f) {
        server.streamFile(f, "text/csv");
        f.close();
        Serial.println("📤 Served: " + fname);
        return;
      }
    }
    server.send(404, "text/plain", "File not found");
    return;
  }

  server.send(404, "text/plain", "Not found: " + uri);
}

void startFileServer() {
  server.on("/report", HTTP_GET, []() {
    server.send(200, "text/plain", "Use /report/<filename>");
  });
  server.onNotFound(handleFileServer);
  server.begin();
  Serial.println("📡 File server started");
}

// ---------------- SETTINGS MENU ----------------
void openSettingsMenu() {
  bool running = true;

  while (running) {
    oled.showMessage("Settings", "1:WiFi  2:User", "D:Back");

    while (true) {
      char key = keypad.getKey();
      if (!key) {
        delay(50);
        continue;
      }

      if (key == '1') {
        oled.showStatus("WiFi setup...");
        bool ok = wifiManager.interactiveConnect(&oled, &keypad);
        if (ok) {
          Serial.println("✅ WiFi reconnected: " + wifiManager.getLocalIP());
        } else {
          Serial.println("⚠ WiFi setup cancelled/failed");
        }
        break;
      }

      if (key == '2') {
        if (!users) {
          oled.showMessage("Users", "No user data", "");
          delay(1500);
          break;
        }

        int count = users->getUserCount();
        if (count <= 0) {
          oled.showMessage("Users", "No users", "configured");
          delay(1500);
          break;
        }

        int idx = 0;
        bool selecting = true;

        while (selecting) {
          User u = users->getUserByIndex(idx);
          if (u.id <= 0) {
            idx = (idx + 1) % count;
            continue;
          }

          String line1 = u.name;
          String line2 = "ID:" + String(u.id) + " Age:" + String(u.age);
          oled.showMessage("Select User", line1, line2 + " A/B/#/D");

          while (true) {
            char k = keypad.getKey();
            if (!k) {
              delay(40);
              continue;
            }

            if (k == 'A') {
              idx = (idx - 1 + count) % count;
              break;
            }
            if (k == 'B') {
              idx = (idx + 1) % count;
              break;
            }
            if (k == '#') {
              menu->setSelectedUserId(u.id);
              oled.showMessage("User", "Selected:", u.name);
              delay(1200);
              selecting = false;
              break;
            }
            if (k == 'D') {
              selecting = false;
              break;
            }
          }
        }
        break;
      }

      if (key == 'D') {
        running = false;
        break;
      }
    }
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n╔═══════════════════════════════════╗");
  Serial.println("║   HEALTH MONITORING SYSTEM v2.0   ║");
  Serial.println("╚═══════════════════════════════════╝\n");

  oled.begin();
  oled.showSplash();
  delay(1500);

  keypad.begin();
  audio.begin();

  oled.showStatus("Init SD Card...");
  if (!sd.begin()) {
    oled.showMessage("SD Error!", "Insert card", "");
    Serial.println("❌ SD init failed - logging disabled");
    delay(2000);
  } else {
    Serial.println("✅ SD initialized");
  }

  users = new Users(&sd, "/users.csv");
  users->begin();

  oled.showStatus("WiFi setup...");
  wifiManager.begin();
  bool wifiOk = wifiManager.interactiveConnect(&oled, &keypad);

  if (wifiOk) {
    Serial.println("✅ WiFi: " + wifiManager.getLocalIP());
  } else {
    Serial.println("❌ WiFi not connected");
    oled.showMessage("WiFi", "Not connected", "");
  }
  delay(1500);

  // UPDATED: Make sure .begin() is called!
  twilioSender   = new TwilioSender(TWILIO_SID, TWILIO_TOKEN, TWILIO_WHATSAPP_FROM);
  twilioFeedback = new TwilioFeedback(TWILIO_SID, TWILIO_TOKEN, TWILIO_WHATSAPP_FROM);
  twilioSender->begin();
  twilioFeedback->begin();  // ← CRITICAL: Must call this!

  oled.showStatus("Init sensors...");
  if (!maxSensor.begin()) {
    oled.showMessage("Sensor Error!", "MAX30102 failed", "");
    Serial.println("❌ MAX30102 init failed");
    delay(2000);
  }

  tempSensor.begin();

  menu = new MenuManager(&oled, &keypad);
  menu->begin();

  startFileServer();

  oled.showMessage("Ready", "Use keypad", "");
  audio.playWelcome();

  session.reset();
  lastFeedbackPoll = millis();

  Serial.println("\n✅ System ready");
  Serial.println("══════════════════════════════════════\n");
}

// ---------------- MAIN LOOP ----------------
void loop() {
  server.handleClient();

  switch (currentState) {
    case STATE_WARMUP:
      handleWarmupState();
      break;

    case STATE_MEASURING_HR:
      handleMeasuringState();
      break;

    case STATE_MEASURING_TEMP:
      handleTempState();
      break;

    case STATE_CONTINUOUS_MONITOR:
      handleContinuousMonitor();
      break;

    case STATE_COMPLETE:
      handleCompleteState();
      break;

    case STATE_IDLE:
    default:
      break;
  }

  if (currentState == STATE_IDLE) {
    MenuAction action = menu->loop();

    switch (action) {
      case START_HR_SPO2: {
        session.reset();
        currentState   = STATE_WARMUP;
        stateStartTime = millis();
        oled.showStatus("Warmup...");
        audio.playStartMeasurement();
        maxSensor.startMeasurement();
        Serial.println("\n🟢 Starting HR/SpO2 measurement");
        break;
      }

      case START_TEMP: {
        oled.showStatus("Reading Temp...");
        tempSensor.loop();
        delay(100);  // Give sensor time to read
  
        float tempF = tempSensor.getTemperatureF();  // CHANGED: Use Fahrenheit
        float tempC = tempSensor.getTemperatureC();  // Also show Celsius for reference
  
       String status = tempSensor.getBodyTempStatus();  // NEW: Get status
  
      oled.showMessage("Temperature", 
                   String(tempF, 1) + "°F (" + String(tempC, 1) + "°F)",
                   status);
  
      Serial.printf("🌡 Temperature: %.1f°F / %.1f°C (%s)\n", 
                tempF, tempC, status.c_str());
      delay(3000);
      break;
    }

      case SEND_CSV: {
        int uid = menu->getSelectedUserId();
        sendReportToDoctor(uid);
        break;
      }

      // UPDATED: Better feedback handling
      case VIEW_FEEDBACK: {
        if (!twilioFeedback) {
          oled.showMessage("Error", "Feedback not init", "");
          delay(1500);
          break;
        }
        
        oled.showStatus("Fetching feedback...");
        Serial.println("\n📬 Manually checking for feedback...");
        
        String body = getLatestFeedbackMessage();
        
        if (body.length() == 0) {
          oled.showMessage("Feedback", "No new messages", "");
          Serial.println("ℹ No new feedback messages");
        } else {
          oled.showFeedback(body, "Doctor says:");
          audio.playFeedbackNotification();
          Serial.println("📬 Feedback displayed: " + body);
        }
        delay(3000);
        break;
      }

      case AUDIO_MENU: {
        oled.showMessage("Audio", "1:Help 2:Play", "");
        delay(1500);
        break;
      }

      case SETTINGS: {
        openSettingsMenu();
        break;
      }

      default:
        break;
    }
  }

  // UPDATED: Better periodic feedback polling
  if (twilioFeedback && millis() - lastFeedbackPoll > FEEDBACK_POLL_INTERVAL) {
    lastFeedbackPoll = millis();
    
    Serial.println("\n🔄 Background feedback check...");
    String body = getLatestFeedbackMessage();

    if (body.length() > 0 && currentState == STATE_IDLE) {
      oled.showFeedback(body, "New from doctor");
      audio.playFeedbackNotification();
      Serial.println("📬 Background feedback: " + body);
      delay(3000);
    }
  }

  delay(LOOP_BASE_DELAY_MS);
}