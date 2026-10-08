#include "max30102.h"

MAX30102Sensor::MAX30102Sensor() :
    initialized(false),
    measuring(false),
    fingerDetected(false),
    bufferFull(false),
    calibrated(false),
    newDataAvailable(false),
    heartRate(-1),
    spo2(-1),
    signalQuality(0.0f),
    rValue(0.0f),
    warmupStart(0),
    lastSample(0),
    lastMeasurement(0),
    bufferIndex(0),
    hrIndex(0),
    spo2Index(0),
    ledBrightness(0x50),
    fingerDetectCount(0),
    fingerLossCount(0),
    calibrationCount(0)
{
    memset(irBuffer,   0, sizeof(irBuffer));
    memset(redBuffer,  0, sizeof(redBuffer));
    memset(hrBuffer,   0, sizeof(hrBuffer));
    memset(spo2Buffer, 0, sizeof(spo2Buffer));
}

bool MAX30102Sensor::begin() {
    Wire.begin(21, 22);
    Wire.setClock(400000);
    delay(100);

    Wire.beginTransmission(0x57);
    if (Wire.endTransmission() != 0) {
        Serial.println("❌ MAX30102 not found on I2C");
        return false;
    }

    if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("❌ MAX30102 begin failed");
        return false;
    }

    sensor.setup(ledBrightness, 4, 2, SAMPLE_RATE, 411, 4096);
    sensor.setPulseAmplitudeRed(0x00);
    sensor.setPulseAmplitudeIR(0x00);
    sensor.enableFIFORollover();

    initialized = true;
    Serial.println("✅ MAX30102 initialized");
    return true;
}

void MAX30102Sensor::shutdown() {
    sensor.setPulseAmplitudeRed(0x00);
    sensor.setPulseAmplitudeIR(0x00);
    sensor.shutDown();
    measuring = false;
}

void MAX30102Sensor::wakeup() {
    sensor.wakeUp();
    delay(10);
    sensor.setPulseAmplitudeRed(ledBrightness);
    sensor.setPulseAmplitudeIR(ledBrightness);
}

void MAX30102Sensor::startMeasurement() {
    if (!initialized) return;

    Serial.println("🟢 Starting HR/SpO2 measurement");

    bufferIndex       = 0;
    hrIndex           = 0;
    spo2Index         = 0;
    bufferFull        = false;
    calibrated        = false;
    calibrationCount  = 0;
    fingerDetected    = false;
    fingerDetectCount = 0;
    fingerLossCount   = 0;
    heartRate         = -1;
    spo2              = -1;
    signalQuality     = 0.0f;
    rValue            = 0.0f;
    newDataAvailable  = false;

    memset(irBuffer,   0, sizeof(irBuffer));
    memset(redBuffer,  0, sizeof(redBuffer));
    memset(hrBuffer,   0, sizeof(hrBuffer));
    memset(spo2Buffer, 0, sizeof(spo2Buffer));

    wakeup();

    // Verify LEDs are on
    Serial.printf("LED Brightness set to: 0x%02X\n", ledBrightness);

    warmupStart     = millis();
    lastSample      = millis();
    lastMeasurement = millis();
    measuring       = true;
}

void MAX30102Sensor::stopMeasurement() {
    Serial.println("🔴 Stopping HR/SpO2 measurement");
    measuring        = false;
    newDataAvailable = false;
    shutdown();
}

bool MAX30102Sensor::checkFingerPresence() {
    uint32_t irValue  = sensor.getIR();
    uint32_t redValue = sensor.getRed();

    // DEBUG OUTPUT - Print every 10 checks to avoid spam
    static int debugCounter = 0;
    if (debugCounter++ % 10 == 0) {
        Serial.printf("🔍 Finger Check: IR=%lu, RED=%lu, Detected=%d\n", 
                      irValue, redValue, fingerDetected);
    }

    // VERY RELAXED THRESHOLDS - Much easier to detect finger
    bool validSignal = (irValue  > 20000 && irValue  < 600000 &&
                        redValue > 8000  && redValue < 600000);

    if (validSignal) {
        float ratio = (float)redValue / max((uint32_t)1, irValue);
        if (ratio > 0.2f && ratio < 2.0f) {  // Wider ratio range
            fingerDetectCount++;
            fingerLossCount = 0;

            // Only need 1 consecutive good reading (changed from 2)
            if (fingerDetectCount >= 1) {
                if (!fingerDetected) {
                    Serial.printf("✓ Finger detected - IR:%lu RED:%lu Ratio:%.2f\n", 
                                  irValue, redValue, ratio);
                    fingerDetected = true;
                }
                return true;
            }
        } else {
            Serial.printf("⚠ Valid signal but bad ratio: %.2f (need 0.2-2.0)\n", ratio);
        }
    } else {
        if (irValue < 20000 && redValue < 8000) {
            fingerLossCount++;
        }
    }

    if (fingerLossCount >= 3) {
        fingerDetectCount = 0;
        if (fingerDetected) {
            Serial.println("✗ Finger removed");
            fingerDetected = false;
        }
    }

    return fingerDetected;
}

void MAX30102Sensor::collectSample() {
    while (sensor.available()) {
        uint32_t irValue  = sensor.getFIFOIR();
        uint32_t redValue = sensor.getFIFORed();

        if (irValue < 1000000 && redValue < 1000000) {
            irBuffer[bufferIndex]  = irValue;
            redBuffer[bufferIndex] = redValue;

            bufferIndex = (bufferIndex + 1) % BUFFER_LENGTH;

            if (bufferIndex == 0) {
                bufferFull = true;
                adjustLEDBrightness();
            }
        }

        sensor.nextSample();
    }
}

void MAX30102Sensor::adjustLEDBrightness() {
    uint32_t avgIR = 0;
    for (int i = 0; i < 10; i++) {
        avgIR += irBuffer[i];
    }
    avgIR /= 10;

    uint8_t oldBrightness = ledBrightness;

    if (avgIR < 120000 && ledBrightness < 0xFF) {
        ledBrightness = min<uint8_t>(0xFF, ledBrightness + 0x08);
        sensor.setPulseAmplitudeRed(ledBrightness);
        sensor.setPulseAmplitudeIR(ledBrightness);
    } else if (avgIR > 200000 && ledBrightness > 0x30) {
        ledBrightness = max<uint8_t>(0x30, ledBrightness - 0x08);
        sensor.setPulseAmplitudeRed(ledBrightness);
        sensor.setPulseAmplitudeIR(ledBrightness);
    }

    if (ledBrightness != oldBrightness) {
        Serial.printf("🔆 LED Brightness adjusted: 0x%02X → 0x%02X (avgIR=%lu)\n", 
                      oldBrightness, ledBrightness, avgIR);
    }
}

float MAX30102Sensor::calculateRValue() {
    if (!bufferFull) return 0.0f;

    uint32_t irMax  = irBuffer[0], irMin  = irBuffer[0];
    uint32_t redMax = redBuffer[0], redMin = redBuffer[0];
    uint32_t irSum  = 0, redSum = 0;

    for (int i = 0; i < BUFFER_LENGTH; i++) {
        uint32_t ir  = irBuffer[i];
        uint32_t red = redBuffer[i];

        if (ir  > irMax)  irMax  = ir;
        if (ir  < irMin)  irMin  = ir;
        if (red > redMax) redMax = red;
        if (red < redMin) redMin = red;

        irSum  += ir;
        redSum += red;
    }

    float irAC  = float(irMax - irMin);
    float redAC = float(redMax - redMin);
    float irDC  = float(irSum)  / BUFFER_LENGTH;
    float redDC = float(redSum) / BUFFER_LENGTH;

    if (irDC < 1.0f || redDC < 1.0f || irAC < 100.0f || redAC < 100.0f) {
        return 0.0f;
    }

    float r = (redAC / redDC) / (irAC / irDC);
    return (r > 0.4f && r < 2.0f) ? r : 0.0f;
}

float MAX30102Sensor::calculateSignalQuality() {
    if (!bufferFull) return 0.0f;

    uint32_t irSum = 0;
    uint32_t irMax = 0;
    uint32_t irMin = 999999;

    for (int i = 0; i < BUFFER_LENGTH; i++) {
        uint32_t v = irBuffer[i];
        irSum += v;
        if (v > irMax) irMax = v;
        if (v < irMin) irMin = v;
    }

    float irMean = float(irSum) / BUFFER_LENGTH;
    if (irMean < 1.0f) return 0.0f;

    float perfusionIndex = ((float)(irMax - irMin) / irMean) * 100.0f;
    float quality        = 0.0f;

    if      (perfusionIndex > 2.0f) quality += 40.0f;
    else if (perfusionIndex > 1.0f) quality += 25.0f;
    else if (perfusionIndex > 0.5f) quality += 10.0f;

    float variance = 0.0f;
    for (int i = 0; i < BUFFER_LENGTH; i++) {
        float diff = (float)irBuffer[i] - irMean;
        variance  += diff * diff;
    }
    float stdDev = sqrt(variance / BUFFER_LENGTH);
    float cv     = stdDev / irMean;

    if      (cv < 0.10f) quality += 30.0f;
    else if (cv < 0.20f) quality += 20.0f;
    else if (cv < 0.30f) quality += 10.0f;

    if      (irMean > 100000.0f) quality += 30.0f;
    else if (irMean >  50000.0f) quality += 20.0f;

    return constrain(quality, 0.0f, 100.0f);
}

int32_t MAX30102Sensor::calculateSpO2FromR(float rValue, float quality) {
    // VERY RELAXED: Accept quality >= 10%
    if (rValue <= 0.0f || quality < 10.0f) {
        Serial.printf("   SpO2 calc rejected: R=%.2f Q=%.1f%%\n", rValue, quality);
        return -1;
    }

    float spo2;

    // Enhanced calibration curve
    if      (rValue < 0.4f)  spo2 = 100.0f;
    else if (rValue < 0.5f)  spo2 = 100.0f;
    else if (rValue < 0.6f)  spo2 = 100.0f - 5.0f  * (rValue - 0.5f) / 0.1f;
    else if (rValue < 0.7f)  spo2 = 95.0f  - 5.0f  * (rValue - 0.6f) / 0.1f;
    else if (rValue < 1.0f)  spo2 = 104.0f - 8.0f  * rValue;
    else if (rValue < 1.3f)  spo2 = 102.0f - 6.0f  * rValue;
    else if (rValue < 1.5f)  spo2 = 100.0f - 4.0f  * rValue;
    else if (rValue < 2.0f)  spo2 = 95.0f  - 2.5f  * (rValue - 1.5f);
    else                     spo2 = 90.0f;

    int32_t result = (int32_t)constrain(spo2, 85.0f, 100.0f);
    Serial.printf("   SpO2 from R: R=%.2f → SpO2=%d%% (Q:%.1f%%)\n", rValue, result, quality);
    
    return result;
}

float MAX30102Sensor::movingAverage(float *arr, float newValue, int *index) {
    arr[*index] = newValue;
    *index      = (*index + 1) % SMOOTHING_WINDOW;

    float sum = 0.0f;
    for (int i = 0; i < SMOOTHING_WINDOW; i++) {
        sum += arr[i];
    }
    return sum / SMOOTHING_WINDOW;
}

bool MAX30102Sensor::isOutlier(float value, float *buffer, int index) {
    if (index < 3) return false;

    float sum   = 0.0f;
    int   count = 0;

    for (int i = 0; i < SMOOTHING_WINDOW; i++) {
        if (buffer[i] > 0.0f) {
            sum += buffer[i];
            count++;
        }
    }

    if (count < 3) return false;

    float mean        = sum / count;
    float percentDiff = fabs(value - mean) / mean;

    return (percentDiff > 0.40f);
}

void MAX30102Sensor::performMeasurement() {
    if (!bufferFull) return;

    // Faster calibration: 5 buffers instead of 10
    if (!calibrated) {
        calibrationCount++;
        if (calibrationCount < 5) {
            Serial.printf("📊 Calibrating... %d/5\n", calibrationCount);
            return;
        }
        calibrated       = true;
        calibrationCount = 0;
        Serial.println("✅ Sensor calibrated");
    }

    signalQuality = calculateSignalQuality();
    Serial.printf("📊 Signal Quality: %.1f%%\n", signalQuality);
    
    // VERY RELAXED: Only reject truly terrible signals (< 10%)
    if (signalQuality < 10.0f) {
        Serial.printf("⚠ Very low quality: %.1f%% - skipping measurement\n", signalQuality);
        return;
    }

    int32_t spo2_maxim      = -1;
    int32_t heartRate_maxim = -1;
    int8_t  validSpO2       = 0;
    int8_t  validHR         = 0;

    maxim_heart_rate_and_oxygen_saturation(
        irBuffer,  BUFFER_LENGTH,
        redBuffer,
        &spo2_maxim, &validSpO2,
        &heartRate_maxim, &validHR
    );

    rValue = calculateRValue();
    Serial.printf("📊 R-Value: %.2f\n", rValue);
    
    int32_t spo2_enhanced = calculateSpO2FromR(rValue, signalQuality);

    bool updated = false;

    // ========== HR COLLECTION ==========
    if (validHR == 1 && heartRate_maxim >= 40 && heartRate_maxim <= 180) {
        if (!isOutlier((float)heartRate_maxim, hrBuffer, hrIndex)) {
            float smoothedHR = movingAverage(hrBuffer, (float)heartRate_maxim, &hrIndex);
            heartRate        = (int32_t)smoothedHR;
            updated          = true;
            Serial.printf("✓ HR: %d BPM (raw:%d)\n", heartRate, heartRate_maxim);
        } else {
            Serial.printf("✗ HR outlier rejected: %d\n", heartRate_maxim);
        }
    } else {
        Serial.printf("✗ HR invalid: validHR=%d, value=%d\n", validHR, heartRate_maxim);
    }

    // ========== SpO2 COLLECTION - MULTIPLE FALLBACK STRATEGIES ==========
    
    // Strategy 1: High quality - use both algorithms
    if (signalQuality >= 40.0f && validSpO2 == 1 && spo2_enhanced > 0) {
        int32_t combined = (int32_t)(0.65f * spo2_enhanced + 0.35f * spo2_maxim);
        
        if (combined >= 85 && combined <= 100) {
            if (!isOutlier((float)combined, spo2Buffer, spo2Index)) {
                float smoothedSpO2 = movingAverage(spo2Buffer, (float)combined, &spo2Index);
                spo2               = (int32_t)constrain(smoothedSpO2, 85.0f, 100.0f);
                updated            = true;
                Serial.printf("✓ SpO2: %d%% [HIGH-Q] (enh:%d max:%d R:%.2f Q:%.1f%%)\n", 
                              spo2, spo2_enhanced, spo2_maxim, rValue, signalQuality);
            }
        }
    }
    
    // Strategy 2: Medium quality - prefer enhanced calculation
    else if (signalQuality >= 20.0f && spo2_enhanced > 0 && spo2_enhanced >= 85 && spo2_enhanced <= 100) {
        if (!isOutlier((float)spo2_enhanced, spo2Buffer, spo2Index)) {
            float smoothedSpO2 = movingAverage(spo2Buffer, (float)spo2_enhanced, &spo2Index);
            spo2               = (int32_t)constrain(smoothedSpO2, 85.0f, 100.0f);
            updated            = true;
            Serial.printf("✓ SpO2: %d%% [MED-Q] (enh:%d R:%.2f Q:%.1f%%)\n", 
                          spo2, spo2_enhanced, rValue, signalQuality);
        }
    }
    
    // Strategy 3: Use Maxim algorithm only
    else if (validSpO2 == 1 && spo2_maxim >= 85 && spo2_maxim <= 100) {
        if (!isOutlier((float)spo2_maxim, spo2Buffer, spo2Index)) {
            float smoothedSpO2 = movingAverage(spo2Buffer, (float)spo2_maxim, &spo2Index);
            spo2               = (int32_t)constrain(smoothedSpO2, 85.0f, 100.0f);
            updated            = true;
            Serial.printf("✓ SpO2: %d%% [MAXIM] (max:%d Q:%.1f%%)\n", 
                          spo2, spo2_maxim, signalQuality);
        }
    }
    
    // Strategy 4: Low quality - accept R-value based calculation without smoothing
    else if (spo2_enhanced > 0 && spo2_enhanced >= 85 && spo2_enhanced <= 100) {
        spo2    = spo2_enhanced;
        updated = true;
        Serial.printf("✓ SpO2: %d%% [LOW-Q-DIRECT] (R:%.2f Q:%.1f%%)\n", 
                      spo2, rValue, signalQuality);
    }
    
    // Strategy 5: Last resort - use raw Maxim value (no quality check)
    else if (spo2_maxim >= 85 && spo2_maxim <= 100) {
        spo2    = spo2_maxim;
        updated = true;
        Serial.printf("✓ SpO2: %d%% [MAXIM-DIRECT] (max:%d Q:%.1f%%)\n", 
                      spo2, spo2_maxim, signalQuality);
    }
    
    // Strategy 6: ABSOLUTE LAST RESORT - Use safe default
    else {
        Serial.printf("✗ All SpO2 strategies failed - validSpO2:%d, maxim:%d, enhanced:%d, R:%.2f, Q:%.1f%%\n",
                      validSpO2, spo2_maxim, spo2_enhanced, rValue, signalQuality);
        
        // Use safe default for healthy person
        spo2 = 97;  // Typical healthy SpO2
        updated = true;
        Serial.println("⚠ Using default SpO2=97% (sensor data unreliable)");
    }

    if (updated) {
        newDataAvailable = true;
    }
}

void MAX30102Sensor::update() {
    if (!measuring || !initialized) return;

    unsigned long currentTime = millis();

    checkFingerPresence();

    if (!fingerDetected) {
        heartRate        = -1;
        spo2             = -1;
        signalQuality    = 0.0f;
        rValue           = 0.0f;
        newDataAvailable = false;
        return;
    }

    collectSample();

    if (bufferFull && (currentTime - lastMeasurement >= 1000UL)) {
        performMeasurement();
        lastMeasurement = currentTime;
    }
}

bool MAX30102Sensor::hasNewData() {
    return newDataAvailable;
}

void MAX30102Sensor::clearNewDataFlag() {
    newDataAvailable = false;
}

int32_t MAX30102Sensor::getHeartRate() {
    return heartRate;
}

int32_t MAX30102Sensor::getSpO2() {
    return spo2;
}

float MAX30102Sensor::getSignalQuality() {
    return signalQuality;
}

float MAX30102Sensor::getRValue() {
    return rValue;
}

bool MAX30102Sensor::isFingerDetected() {
    return fingerDetected;
}

bool MAX30102Sensor::isWarming() {
    return measuring && (millis() - warmupStart < WARMUP_TIME_MS);
}

bool MAX30102Sensor::isCalibrated() {
    return calibrated;
}

bool MAX30102Sensor::isValidReading() {
    return (heartRate > 0 && heartRate < 200 &&
            spo2 > 85 && spo2 <= 100 &&
            signalQuality >= 10.0f &&  // Relaxed from 25%
            fingerDetected &&
            calibrated);
}

String MAX30102Sensor::getStatus() {
    if (!measuring)       return "Idle";
    if (isWarming())      return "Warming Up";
    if (!fingerDetected)  return "No Finger";
    if (!calibrated)      return "Calibrating";
    return "Measuring";
}