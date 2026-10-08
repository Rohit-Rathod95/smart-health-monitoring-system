#include "users.h"
#include "sdmanager.h"
#include <Arduino.h>

Users::Users(SDManager *sd, const String &csvPath)
  : sdm(sd),
    path(csvPath),
    list() {}

bool Users::begin() {
  return load();
}

void Users::ensureDefaultUser() {
  if (!sdm) return;

  if (list.empty()) {
    // Create a default user if file was missing or empty
    User u;
    u.id     = 1;
    u.name   = "DefaultUser";
    u.phone  = "+910000000000";
    u.age    = 25;
    u.gender = "Male";
    list.push_back(u);

    // Also write to CSV so it's persistent
    String header  = "id,name,phone,age,gender\n";
    String line    = "1,DefaultUser,+910000000000,25,Male\n";
    sdm->saveCSV(path, header + line);
  }
}

bool Users::load() {
  if (!sdm) {
    Serial.println("❌ Users: SDManager not set");
    return false;
  }

  list.clear();

  // If file doesn't exist, create a default one
  if (!sdm->fileExists(path)) {
    Serial.println("ℹ Users: creating default users CSV");
    String header  = "id,name,phone,age,gender\n";
    String defaultLine = "1,Rohit,+919876543210,25,Male\n";
    sdm->saveCSV(path, header + defaultLine);
  }

  String content = sdm->readFile(path);
  if (content.length() == 0) {
    Serial.println("⚠ Users: CSV empty, adding default user");
    ensureDefaultUser();
    return !list.empty();
  }

  int idx    = 0;
  int lineNo = 0;

  while (idx < (int)content.length()) {
    int nl = content.indexOf('\n', idx);
    if (nl == -1) nl = content.length();

    String line = content.substring(idx, nl);
    idx         = nl + 1;
    line.trim();

    if (line.length() == 0) {
      continue;
    }

    if (lineNo == 0) {
      // Header line
      lineNo++;
      continue;
    }

    // Parse CSV fields "id,name,phone,age,gender"
    int c1 = line.indexOf(',');
    int c2 = (c1 >= 0) ? line.indexOf(',', c1 + 1) : -1;
    int c3 = (c2 >= 0) ? line.indexOf(',', c2 + 1) : -1;
    int c4 = (c3 >= 0) ? line.indexOf(',', c3 + 1) : -1;

    if (c1 == -1 || c2 == -1) {
      // Must at least have id and name and phone
      continue;
    }

    User u;
    u.id   = line.substring(0, c1).toInt();
    u.name = line.substring(c1 + 1, c2);
    u.name.trim();

    if (c3 == -1) {
      // Only phone present after name
      u.phone  = line.substring(c2 + 1);
      u.phone.trim();
      u.age    = 25;
      u.gender = "Male";
    } else if (c4 == -1) {
      // phone, age present
      u.phone  = line.substring(c2 + 1, c3);
      u.phone.trim();
      u.age    = line.substring(c3 + 1).toInt();
      if (u.age <= 0) u.age = 25;
      u.gender = "Male";
    } else {
      // phone, age, gender present
      u.phone  = line.substring(c2 + 1, c3);
      u.phone.trim();
      u.age    = line.substring(c3 + 1, c4).toInt();
      if (u.age <= 0) u.age = 25;
      u.gender = line.substring(c4 + 1);
      u.gender.trim();
    }

    if (u.id <= 0) {
      // ignore invalid IDs
      continue;
    }

    list.push_back(u);
    lineNo++;
  }

  ensureDefaultUser();
  Serial.printf("✅ Loaded %d users from %s\n", (int)list.size(), path.c_str());
  return !list.empty();
}

User Users::getUserById(int id) {
  for (size_t i = 0; i < list.size(); i++) {
    if (list[i].id == id) {
      return list[i];
    }
  }

  User empty;
  empty.id     = -1;
  empty.name   = "";
  empty.phone  = "";
  empty.age    = 0;
  empty.gender = "";
  return empty;
}

UserProfile* Users::getUserProfileById(int id) {
  static UserProfile profile;

  for (size_t i = 0; i < list.size(); i++) {
    if (list[i].id == id) {
      profile.user   = &list[i];
      profile.name   = list[i].name;
      profile.age    = list[i].age;
      profile.gender = list[i].gender;
      return &profile;
    }
  }

  return nullptr;
}

int Users::defaultUserId() {
  if (!list.empty()) {
    return list[0].id;
  }
  return -1;
}

int Users::getUserCount() {
  return (int)list.size();
}

User Users::getUserByIndex(int index) {
  if (index < 0 || index >= (int)list.size()) {
    User empty;
    empty.id     = -1;
    empty.name   = "";
    empty.phone  = "";
    empty.age    = 0;
    empty.gender = "";
    return empty;
  }
  return list[index];
}

