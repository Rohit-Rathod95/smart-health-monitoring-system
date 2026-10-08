#include "twilio_feedback.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

TwilioFeedback::TwilioFeedback(const String &sid,
                               const String &token,
                               const String &whatsappNumber)
  : accountSid(sid),
    authToken(token),
    fromNumber(whatsappNumber),
    lastMessageSid("") {}

void TwilioFeedback::begin() {
  // Initialize last message SID to empty
  lastMessageSid = "";
  Serial.println("✅ TwilioFeedback initialized");
}

String TwilioFeedback::fetchLatest() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ TwilioFeedback: WiFi not connected");
    return "";
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;

  // FIXED: Query messages FROM doctor TO your WhatsApp number
  // PageSize=1 gets only the most recent message
  String url = "https://api.twilio.com/2010-04-01/Accounts/" +
               accountSid +
               "/Messages.json?From=whatsapp%3A" +
               urlEncode(DOCTOR_PHONE) +
               "&To=whatsapp%3A" +
               urlEncode(fromNumber) +
               "&PageSize=1";

  Serial.println("📡 Fetching feedback from: " + url);

  if (!https.begin(client, url)) {
    Serial.println("❌ TwilioFeedback: HTTPS begin failed");
    return "";
  }

  https.setAuthorization(accountSid.c_str(), authToken.c_str());
  https.setTimeout(15000);

  int code = https.GET();
  String body = "";

  if (code == 200) {
    body = https.getString();
    Serial.println("✅ TwilioFeedback: Got response");
    Serial.println("Raw JSON: " + body.substring(0, 200) + "...");
  } else {
    Serial.printf("❌ TwilioFeedback: HTTP GET failed, code=%d\n", code);
    if (code > 0) {
      Serial.println("Error body: " + https.getString());
    }
  }

  https.end();
  return body;
}

String TwilioFeedback::parseLatestMessage(const String& json) {
  if (json.length() == 0) {
    Serial.println("⚠ TwilioFeedback: Empty JSON");
    return "";
  }

  // Use ArduinoJson for proper parsing
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("❌ JSON parse failed: ");
    Serial.println(error.c_str());
    return "";
  }

  // Check if messages array exists and has items
  if (!doc.containsKey("messages")) {
    Serial.println("⚠ No 'messages' key in JSON");
    return "";
  }

  JsonArray messages = doc["messages"].as<JsonArray>();
  if (messages.size() == 0) {
    Serial.println("ℹ No messages found");
    return "";
  }

  // Get the first (most recent) message
  JsonObject msg = messages[0];
  
  String sid = msg["sid"] | "";
  String body = msg["body"] | "";
  String status = msg["status"] | "";

  Serial.println("📨 Message SID: " + sid);
  Serial.println("📨 Message body: " + body);
  Serial.println("📨 Status: " + status);

  // Check if this is a new message
  if (sid.length() > 0 && sid != lastMessageSid) {
    lastMessageSid = sid;
    Serial.println("✨ New message detected!");
    return body;
  } else {
    Serial.println("ℹ No new messages (already seen)");
    return "";
  }
}

String TwilioFeedback::urlEncode(const String& str) {
  String encoded = "";
  char c;
  for (size_t i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == '+') {
      encoded += "%2B";
    } else if (c == ' ') {
      encoded += "%20";
    } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      encoded += '%';
      encoded += String(c, HEX);
    }
  }
  return encoded;
}