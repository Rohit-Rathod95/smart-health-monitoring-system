#ifndef TWILIO_FEEDBACK_H
#define TWILIO_FEEDBACK_H

#include <Arduino.h>
extern const String DOCTOR_PHONE;

class TwilioFeedback {
public:
  TwilioFeedback(const String &sid, const String &token, const String &whatsappNumber);

  void begin();

  // Fetch latest inbound message from doctor
  // Returns full JSON string from Twilio API, or empty string on error
  String fetchLatest();

  // Parse JSON response and extract message body
  // Returns message body if new, empty string if already seen or error
  String parseLatestMessage(const String& json);

private:
  String accountSid;
  String authToken;
  String fromNumber;      // The Twilio WhatsApp number
  String lastMessageSid;  // Track last seen message to avoid duplicates

  String urlEncode(const String& str);
};

#endif