# Smart Health Monitoring System

An ESP32-based health-monitoring device that measures heart rate, blood oxygen saturation (SpO2), and body temperature. The device presents readings on an SSD1306 OLED, accepts commands through a 4x4 matrix keypad, stores measurements on an SD card, and can notify a doctor through Twilio WhatsApp.

> **Important:** This project is an educational/prototyping system. It is not a medical device and must not be used to diagnose, treat, or rule out a medical condition. Sensor readings can be affected by placement, motion, ambient conditions, hardware quality, and firmware limitations.

## Features

- Heart-rate and SpO2 measurement using a MAX30102/MAX30105-compatible optical sensor.
- Body-temperature measurement using a DS18B20 OneWire sensor.
- SSD1306 128x64 OLED user interface.
- 4x4 matrix keypad with debounced, one-event-per-press input.
- DFPlayer-style audio feedback for startup, measurement, completion, errors, and messages.
- User profiles loaded from `/users.csv` on the SD card.
- Raw measurement logging and cycle logging as CSV files.
- CSV report generation and serving through an ESP32 HTTP server.
- Wi-Fi network scanning and interactive connection setup from the keypad.
- Twilio WhatsApp alerts for abnormal readings.
- Twilio WhatsApp report delivery with a report URL.
- Polling and display of inbound doctor feedback messages.
- Age-based heart-rate thresholds and fixed SpO2/temperature thresholds.

## How the system works

At startup, the firmware initializes the display, keypad, audio module, SD card, users, Wi-Fi, Twilio clients, and sensors. The device then shows the main menu:

| Key | Action |
| --- | --- |
| `A` | Start heart-rate and SpO2 measurement |
| `B` | Take a temperature reading |
| `C` | Send the selected user's CSV report to the doctor |
| `D` | Fetch and display the latest doctor feedback |
| `1` | Open the audio menu |
| `2` | Open settings |

The settings menu provides:

- `1` - Scan for Wi-Fi networks, select one with `A`/`B`, confirm with `#`, or cancel with `D`.
- `2` - Browse users with `A`/`B`, select with `#`, or cancel with `D`.
- `D` - Return to the main menu.

### Measurement flow

1. Select a user in **Settings > User**.
2. Press `A` and place a finger on the optical sensor.
3. The device performs a 10-second warm-up.
4. Heart rate and SpO2 are sampled for up to 60 seconds, while readings are shown and logged.
5. Press `*` to stop the optical measurement early.
6. The device performs a 60-second temperature phase.
7. The final averages are shown on the OLED.
8. If a reading is outside the configured safe range, an alert is sent to the configured doctor number.
9. From the final screen:
   - `*` returns to the menu.
   - `#` repeats the measurement.
   - `C` sends the CSV report.

Temperature can also be read directly with `B`. The firmware refreshes the DS18B20 approximately every two seconds and uses smoothing for valid readings.

## Repository layout

```text
.
├── README.md
└── final_system/
    ├── final_system.ino       # Arduino entry point, state machine, configuration
    ├── oled.{h,cpp}           # SSD1306 display abstraction
    ├── keypad.{h,cpp}         # 4x4 keypad scanning and debouncing
    ├── audio.{h,cpp}          # DFPlayer-style UART commands
    ├── max30102.{h,cpp}       # Heart-rate and SpO2 acquisition/calculation
    ├── temperature.{h,cpp}    # DS18B20 acquisition and temperature status
    ├── sdmanager.{h,cpp}      # SD-card files and CSV logging
    ├── users.{h,cpp}          # User CSV loading and profile lookup
    ├── wifi_manager.{h,cpp}   # Wi-Fi scanning and interactive connection
    ├── twilio_sender.{h,cpp}  # Outbound WhatsApp messages and media
    ├── twilio_feedback.{h,cpp}# Inbound WhatsApp feedback polling
    └── menu.{h,cpp}           # Main menu actions and selected user
```

The repository currently contains an Arduino sketch rather than a PlatformIO project. The recommended build workflow is therefore Arduino IDE or Arduino CLI with the ESP32 board package.

## Hardware

The firmware uses the following default ESP32 pin assignments:

| Component | Signal | ESP32 pin |
| --- | --- | ---: |
| SSD1306 OLED | I2C SDA | GPIO 21 |
| SSD1306 OLED | I2C SCL | GPIO 22 |
| MAX30102 | I2C SDA/SCL | GPIO 21 / GPIO 22 |
| DS18B20 | OneWire data | GPIO 0 |
| SD card | Chip select | GPIO 5 |
| DFPlayer/audio module | RX | GPIO 16 |
| DFPlayer/audio module | TX | GPIO 17 |
| Keypad | Row 1 | GPIO 25 |
| Keypad | Row 2 | GPIO 26 |
| Keypad | Row 3 | GPIO 33 |
| Keypad | Row 4 | GPIO 32 |
| Keypad | Column 1 | GPIO 13 |
| Keypad | Column 2 | GPIO 12 |
| Keypad | Column 3 | GPIO 14 |
| Keypad | Column 4 | GPIO 27 |

The keypad layout expected by the firmware is:

```text
[ 1 ] [ 2 ] [ 3 ] [ A ]
[ 4 ] [ 5 ] [ 6 ] [ B ]
[ 7 ] [ 8 ] [ 9 ] [ C ]
[ * ] [ 0 ] [ # ] [ D ]
```

### Wiring notes

- Use a common ground between the ESP32 and every peripheral.
- Confirm the voltage requirements of the OLED, sensors, SD module, and audio module before wiring them to the ESP32.
- Use the correct pull-up arrangement for the DS18B20 data line as required by the sensor/module.
- Use a FAT-compatible SD card and insert it before powering the device.
- The firmware expects the OLED at I2C address `0x3C`.
- The MAX30102/MAX30105-compatible sensor is expected at I2C address `0x57`.
- GPIO 0 is a boot-strapping pin on many ESP32 boards. If the DS18B20 circuit interferes with boot, move the sensor to another GPIO and update `ONEWIRE_PIN`.

## Software prerequisites

Install:

1. Arduino IDE 2.x, Arduino CLI, or an equivalent Arduino-compatible environment.
2. Espressif ESP32 board support package.
3. The libraries below through the Arduino Library Manager or your package manager:

| Library | Used for |
| --- | --- |
| Adafruit GFX Library | OLED graphics primitives |
| Adafruit SSD1306 | SSD1306 OLED driver |
| SparkFun MAX3010x Sensor Library | MAX30102/MAX30105 access and helper algorithms |
| OneWire | DS18B20 bus communication |
| DallasTemperature | DS18B20 temperature handling |
| ArduinoJson | Twilio feedback JSON parsing |

The following are supplied by the ESP32 Arduino core or standard Arduino libraries:

- `Arduino.h`
- `WiFi.h`
- `WebServer.h`
- `WiFiClientSecure.h`
- `HTTPClient.h`
- `Wire.h`
- `SPI.h`
- `SD.h`
- `HardwareSerial.h`

## Installation and upload

### Arduino IDE

1. Clone or download this repository.
2. Open `final_system/final_system.ino` in Arduino IDE.
3. Select an ESP32 board matching your hardware, for example **ESP32 Dev Module**.
4. Install the libraries listed above.
5. Configure the credentials in `final_system.ino` as described in the next section.
6. Insert a formatted SD card.
7. Select the correct serial port.
8. Compile and upload.
9. Open Serial Monitor at **115200 baud**.
10. On first boot, use the keypad to select a Wi-Fi network and enter its password.

All files in `final_system/` should remain in the same sketch directory so the Arduino build system can find the local headers and source files.

### Arduino CLI example

The exact fully-qualified board name depends on the installed ESP32 core and board. A typical workflow is:

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "SparkFun MAX3010x Sensor Library"
arduino-cli lib install "OneWire"
arduino-cli lib install "DallasTemperature"
arduino-cli lib install "ArduinoJson"
arduino-cli compile --fqbn esp32:esp32:esp32 final_system
arduino-cli upload -p <PORT> --fqbn esp32:esp32:esp32 final_system
```

Adjust the FQBN and port for the selected ESP32 board.

## Configuration

The configuration constants are currently at the top of [`final_system.ino`](final_system/final_system.ino):

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

const String TWILIO_SID = "YOUR_TWILIO_SID";
const String TWILIO_TOKEN = "YOUR_TWILIO_TOKEN";
const String TWILIO_WHATSAPP_FROM = "YOUR_TWILIO_WHATSAPP_NUMBER";
const String DOCTOR_PHONE = "YOUR_DOCTOR_PHONE";
```

Replace these placeholders before uploading:

- `WIFI_SSID` and `WIFI_PASS` are retained as firmware configuration values, although the normal startup flow uses interactive Wi-Fi selection.
- `TWILIO_SID` is the Twilio Account SID.
- `TWILIO_TOKEN` is the Twilio Auth Token.
- `TWILIO_WHATSAPP_FROM` is the Twilio WhatsApp sender number, without a duplicate `whatsapp:` prefix.
- `DOCTOR_PHONE` is the destination phone number in international format, also without a duplicate `whatsapp:` prefix.

### Credential safety

- Do not commit real Wi-Fi or Twilio credentials.
- Do not paste credentials into issue reports or screenshots.
- For a production-quality version, move secrets out of the sketch and inject them through a protected build configuration or a device provisioning flow.
- The current Twilio HTTPS implementation calls `setInsecure()`. This disables certificate validation and is suitable only for testing. Production deployments should validate the Twilio certificate chain.

## SD-card files and data format

On initialization, the firmware creates `/logs` if it does not already exist. User data is stored in `/users.csv`.

### User file

Create `/users.csv` with this header and one user per row:

```csv
id,name,phone,age,gender
1,Example User,+15551234567,35,Other
```

The parser requires at least an ID, name, and phone. Age and gender are optional; missing or invalid age values default to `25`, and missing gender defaults to `Male`.

If `/users.csv` is missing or empty, the firmware creates a default user automatically.

### Measurement files

For user ID `1`, the firmware creates:

```text
/logs/user_1_raw.csv
/logs/user_1_cycles.csv
/logs/user_1_report_<timestamp>.csv
```

Raw measurements use:

```csv
timestamp,hr,spo2,temp
```

Continuous-monitoring cycles use:

```csv
cycle,timestamp,avg_hr,avg_spo2,avg_temp
```

The report endpoint serves a copy of the user's raw CSV log.

## Alerts, reports, and local file serving

### Abnormal-reading alerts

The firmware checks the final averages against:

- Heart rate: age-dependent range, generally 60-100 BPM for ages 13-55.
- SpO2: minimum `95%`.
- Temperature: `95°F` to `101°F`.

These values are firmware thresholds, not clinical guidance. Review and validate them with a qualified medical professional before relying on the system.

### WhatsApp reports

When the device has Wi-Fi and valid Twilio credentials, it sends the doctor:

- An abnormal-reading alert when a threshold is crossed.
- A summary report when the user presses `C` from the main menu or final-results screen.

The report message includes a URL pointing to the ESP32's local web server. The doctor must be able to reach that IP address for the CSV media link to work. A device IP on a private LAN is generally not reachable from the public internet.

### Local report endpoint

After Wi-Fi connection, the device starts an HTTP server on port `80`:

```text
http://<device-ip>/report
http://<device-ip>/report/<filename>
```

The first endpoint explains the expected path. The second streams a CSV file from `/logs`. This server has no authentication; use it only on a trusted network.

## Serial diagnostics

Open the Serial Monitor at `115200` baud. Startup and runtime logs include:

- Peripheral initialization status.
- Wi-Fi connection and assigned IP address.
- MAX30102 finger detection, signal quality, HR, and SpO2 diagnostics.
- DS18B20 temperature readings.
- SD logging status.
- Twilio HTTP status codes and response bodies.
- Report files served by the local HTTP server.

Useful symptoms:

| Symptom | Checks |
| --- | --- |
| OLED does not initialize | Confirm power, ground, I2C wiring, and address `0x3C`. |
| MAX30102 is not found | Confirm I2C wiring, address `0x57`, power, and sensor module compatibility. |
| Finger is not detected | Keep the finger still, cover the sensor correctly, and inspect IR/RED debug values. |
| Temperature shows no reading | Confirm DS18B20 wiring, pull-up resistor, GPIO assignment, and sensor presence. |
| SD logging is disabled | Confirm FAT formatting, card insertion, CS wiring, and GPIO 5 configuration. |
| Wi-Fi cannot connect | Check the selected SSID/password and use the settings menu to retry. |
| WhatsApp delivery fails | Check Wi-Fi, Twilio credentials, WhatsApp sender setup, recipient format, and Serial Monitor HTTP codes. |
| CSV media cannot be downloaded | Ensure the doctor/network can reach the ESP32's local IP and the report filename is correct. |

## Implementation notes and known limitations

- The main firmware is a state machine with idle, warm-up, HR/SpO2 measurement, temperature, continuous-monitoring, and completion states.
- Sensor readings are smoothed and validated, but the project does not provide calibration, clinical validation, encryption, authentication, or tamper protection.
- Measurement timestamps are based on `millis()`, not wall-clock time.
- The current user CSV parser does not support escaped commas or quoted CSV fields.
- The report generator copies the user's raw log; it does not create a separate calculated-summary table.
- The Twilio feedback poll tracks the last message SID only in RAM, so the same message may be considered new after a reboot.
- Some OLED rendering strings in the current source still use Celsius labels while the measurement flow stores/logs Fahrenheit. Verify the physical display and update labels before production use.
- Memory allocation for several managers occurs dynamically during setup and is not reclaimed because the device is intended to run continuously.

## Development guidance

When modifying the firmware:

1. Keep hardware pin definitions and timing constants near the top of `final_system.ino`.
2. Keep peripheral-specific behavior in its corresponding module rather than expanding the main state machine.
3. Test with the Serial Monitor connected; the diagnostic output is the primary runtime observability mechanism.
4. Test sensor failure paths with peripherals disconnected before testing a complete measurement.
5. Never use real patient data or production credentials while developing.
6. Before deploying hardware changes, verify voltage levels and boot behavior on the actual ESP32 board.

## License

No license file is currently included in the repository. Add an explicit license before distributing the project or incorporating it into another product.
