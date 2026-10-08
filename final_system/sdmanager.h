#ifndef SDMANAGER_H
#define SDMANAGER_H

#include <SD.h>
#include <Arduino.h>

class SDManager {
private:
  int    csPin;
  String currentLogFile;
  bool   initialized;

public:
  SDManager(int cs);

  // Initialize SD card and /logs directory.
  bool begin();

  // File operations
  bool   fileExists(const String &path);
  bool   createDir(const String &path);
  String readFile(const String &path);
  bool   writeFile(const String &path, const String &content);
  bool   appendFile(const String &path, const String &content);
  bool   deleteFile(const String &path);

  // Logging methods
  bool   appendToLog(const String &path, const String &line);
  bool   writeLog(const String &path, const String &line);    // alias wrapper
  bool   saveCSV(const String &path, const String &content);
  bool   logMeasurement(int uid, int hr, int spo2, float temp);
  bool   logCycle(int uid, int cycle, int hr, int spo2, float temp);
  String generateCSVReport(int uid, void* sessionData);
  String getReportUrl(const String &path);

  // Utility
  void     listDir(const String &dirname, uint8_t levels);
  uint64_t getFreeSpace();
};

#endif
