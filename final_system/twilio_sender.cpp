#include "twilio_sender.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFi.h>

TwilioSender::TwilioSender(const String &sid,
                           const String &token,
                           const String &fromNumber)
  : accountSid(sid),
    authToken(token),
    from(fromNumber) {}

void TwilioSender::begin() {
  // No-op for now; placeholder for any future setup
}

// Basic URL-encode helper for x-www-form-urlencoded
String TwilioSender::urlencode(const String &s) {
  String out;
  out.reserve(s.length() * 2);

  for (uint16_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];

    // Unreserved characters according to RFC 3986
    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else if (c == ' ') {
      // Form encoding uses '+' for spaces
      out += '+';
    } else {
      char buf[4];
      sprintf(buf, "%%%02X", c);
      out += buf;
    }
  }

  return out;
}

bool TwilioSender::sendWhatsAppWithMedia(const String &toNumber,
                                         const String &body,
                                         const String &mediaUrl) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ TwilioSender: WiFi not connected");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // NOTE: for testing only. Add CA cert for production.

  HTTPClient https;
  String url = "https://api.twilio.com/2010-04-01/Accounts/" +
               accountSid +
               "/Messages.json";

  if (!https.begin(client, url)) {
    Serial.println("❌ TwilioSender: HTTPS begin failed");
    return false;
  }

  https.setAuthorization(accountSid.c_str(), authToken.c_str());
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  https.setTimeout(10000); // 10s timeout

  // Compose POST body
  String postData;
  postData  = "To=whatsapp%3A"   + urlencode(toNumber);  // +91xxxx
  postData += "&From=whatsapp%3A" + urlencode(from);
  postData += "&Body="           + urlencode(body);

  if (mediaUrl.length() > 0) {
    postData += "&MediaUrl=" + urlencode(mediaUrl);
  }

  int code = https.POST(postData);
  String resp = https.getString();
  https.end();

  Serial.printf("Twilio POST code=%d\n", code);
  // Optional: you can comment this out if it’s too verbose
  Serial.println(resp);

  return (code >= 200 && code < 300);
}
