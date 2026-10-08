#include "sdmanager.h"
#include <SPI.h>

SDManager::SDManager(int cs)
  : csPin(cs),
    currentLogFile(""),
    initialized(false) {}

bool SDManager::begin() {
  if (!SD.begin(csPin)) {
    Serial.println("❌ SD init failed");
    initialized = false;
    return false;
  }

  initialized = true;

  if (!SD.exists("/logs")) {
    SD.mkdir("/logs");
  }

  Serial.println("✅ SD Card initialized");
  return true;
}

// ---------------- Basic file helpers ----------------

bool SDManager::fileExists(const String &path) {
  if (!initialized) return false;
  return SD.exists(path.c_str());
}

bool SDManager::createDir(const String &path) {
  if (!initialized) return false;
  if (SD.exists(path.c_str())) return true;
  return SD.mkdir(path.c_str());
}

String SDManager::readFile(const String &path) {
  if (!initialized) return "";

  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) {
    return "";
  }

  String out;
  while (f.available()) {
    out += (char)f.read();
  }
  f.close();
  return out;
}

bool SDManager::writeFile(const String &path, const String &content) {
  if (!initialized) return false;

  File f = SD.open(path.c_str(), FILE_WRITE);
  if (!f) {
    return false;
  }

  f.print(content);
  f.close();
  return true;
}

bool SDManager::appendFile(const String &path, const String &content) {
  if (!initialized) return false;

  File f = SD.open(path.c_str(), FILE_APPEND);
  if (!f) {
    // fallback: try creating file
    f = SD.open(path.c_str(), FILE_WRITE);
    if (!f) return false;
  }

  f.print(content);
  f.close();
  return true;
}

bool SDManager::deleteFile(const String &path) {
  if (!initialized) return false;
  return SD.remove(path.c_str());
}

// ---------------- Logging helpers ----------------

bool SDManager::appendToLog(const String &path, const String &line) {
  // Append line with newline
  return appendFile(path, line + "\n");
}

bool SDManager::writeLog(const String &path, const String &line) {
  // For backward compatibility: behaves like appendToLog
  return appendToLog(path, line);
}

bool SDManager::saveCSV(const String &path, const String &content) {
  // Overwrite / create CSV file
  return writeFile(path, content);
}

bool SDManager::logMeasurement(int uid, int hr, int spo2, float temp) {
  if (!initialized) return false;

  String filename = "/logs/user_" + String(uid) + "_raw.csv";

  if (!fileExists(filename)) {
    saveCSV(filename, "timestamp,hr,spo2,temp\n");
  }

  String line = String(millis()) + "," +
                String(hr) + "," +
                String(spo2) + "," +
                String(temp, 1);

  return appendToLog(filename, line);
}

bool SDManager::logCycle(int uid, int cycle, int hr, int spo2, float temp) {
  if (!initialized) return false;

  String filename = "/logs/user_" + String(uid) + "_cycles.csv";

  if (!fileExists(filename)) {
    saveCSV(filename, "cycle,timestamp,avg_hr,avg_spo2,avg_temp\n");
  }

  String line = String(cycle) + "," +
                String(millis()) + "," +
                String(hr) + "," +
                String(spo2) + "," +
                String(temp, 1);

  return appendToLog(filename, line);
}

String SDManager::generateCSVReport(int uid, void* /*sessionData*/) {
  if (!initialized) return "";

  String filename = "user_" + String(uid) + "_report_" + String(millis()) + ".csv";
  String fullPath = "/logs/" + filename;

  // Copy raw log if it exists, otherwise create a header-only CSV
  String rawFile = "/logs/user_" + String(uid) + "_raw.csv";
  if (fileExists(rawFile)) {
    String content = readFile(rawFile);
    saveCSV(fullPath, content);
  } else {
    saveCSV(fullPath, "timestamp,hr,spo2,temp\n");
  }

  currentLogFile = fullPath;
  return filename; // note: this is just the bare filename; used by web server as /report/<filename>
}

String SDManager::getReportUrl(const String &path) {
  int lastSlash = path.lastIndexOf('/');
  if (lastSlash >= 0) {
    return "/report/" + path.substring(lastSlash + 1);
  }
  return "/report/" + path;
}

// ---------------- Utility ----------------

void SDManager::listDir(const String &dirname, uint8_t levels) {
  if (!initialized) return;

  Serial.printf("Listing directory: %s\n", dirname.c_str());

  File root = SD.open(dirname.c_str());
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    root.close();
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(String(file.name()), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
  root.close();
}

uint64_t SDManager::getFreeSpace() {
  // Many Arduino SD libs don’t expose free space directly.
  // We return 0 here or you can implement a card-specific check later.
  // This is a safe stub that won't break anything.
  return 0;
}
