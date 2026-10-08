#ifndef USERS_H
#define USERS_H

#include <Arduino.h>
#include <vector>
#include "sdmanager.h"

struct User {
  int    id;
  String name;
  String phone;
  int    age;
  String gender;
};

struct UserProfile {
  User*  user;    // pointer to entry in the internal list
  String name;
  int    age;
  String gender;
};

class Users {
public:
  Users(SDManager *sd, const String &csvPath = "/users.csv");

  // Initialize + load user CSV
  bool begin();

  // Reload users from CSV (clears old list)
  bool load();

  // Simple lookup by ID (returns a copy; id==-1 if not found)
  User getUserById(int id);

  // Returns pointer to a static UserProfile (or nullptr if not found).
  UserProfile* getUserProfileById(int id);

  // First user in list, or -1 if none
  int defaultUserId();

  int getUserCount();
  User getUserByIndex(int index);


private:
  SDManager      *sdm;
  String          path;
  std::vector<User> list;

  void ensureDefaultUser();
};

#endif
