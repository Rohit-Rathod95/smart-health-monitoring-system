#ifndef TWILIO_SENDER_H
#define TWILIO_SENDER_H

#include <Arduino.h>

class TwilioSender {
public:
  TwilioSender(const String &sid, const String &token, const String &fromNumber);

  // No heavy init right now; kept for symmetry and future use
  void begin();

  // Send WhatsApp message (optionally with media URL).
  // toNumber: recipient e.g. +91XXXX (WITHOUT "whatsapp:")
  // body: message text
  // mediaUrl: absolute URL for CSV or images, or "" for no media
  // Returns true if HTTP 2xx, false otherwise.
  bool sendWhatsAppWithMedia(const String &toNumber,
                             const String &body,
                             const String &mediaUrl);

private:
  String accountSid;
  String authToken;
  String from; // Twilio WhatsApp number (e.g. +14155238886)

  String urlencode(const String &s);
};

#endif
