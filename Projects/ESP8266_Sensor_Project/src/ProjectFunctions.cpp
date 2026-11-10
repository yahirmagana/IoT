/*
 * Project/Program Name : ESP8266 Temperature and Sound Logger
 * File Name            : ProjectFunctions.cpp
 * Author               : Yahir Magana
 * Date                 : 2026-11-06
 * Version              : 2.0
 *
 * Purpose:
 * Implements sensor reads, switch detection, time-zone selection, Wi-Fi,
 * HTTPS time synchronization, and reliable HTTPS measurement uploads.
 *
 * Inputs:
 * DS18B20 on D2, sound module AO on A0, pushbutton on D5, tilt switch on D6,
 * serial time-zone choice, and the credentials in Secrets.h.
 *
 * Outputs:
 * One timestamped message per new switch activation, plus status and RSSI on
 * USB serial. Failed messages are retried every 30 seconds with the same data.
 *
 * Example Application:
 * SensorProject::update() checks switch edges, takes readings, and transmits
 * them only after a pushbutton or tilt activation.
 *
 * Dependencies:
 * ESP8266 Arduino core (Wi-Fi, HTTPClient, and BearSSL HTTPS), OneWire,
 * DallasTemperature, and ArduinoJson.
 *
 * Usage Notes:
 * The sound activity percentage is sent as nodeHumidity because the existing
 * database/PHP field is named humidity. setInsecure() encrypts TLS traffic but
 * does not validate the server certificate; see README.md.
 * ---------------------------------------------------------------------------
 */

#include "ProjectFunctions.h"

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include <time.h>

#include "Secrets.h"

namespace {

constexpr uint8_t kSoundPin = A0;
constexpr uint32_t kWiFiRetryMs = 15000UL;
constexpr uint32_t kHttpTimeoutMs = 8000UL;
constexpr uint32_t kMessageRetryMs = 60000UL;
constexpr uint32_t kClockRetryMs = 30000UL;
constexpr uint32_t kClockSyncIntervalMs = 21600000UL;  // Six hours.
constexpr uint32_t kTimeZoneWaitMs = 10000UL;
constexpr uint32_t kSwitchDebounceMs = 120UL;
constexpr uint32_t kRssiPrintMs = 10000UL;

constexpr uint8_t kSoundSampleCount = 96;
constexpr uint16_t kSoundSampleDelayUs = 250;
constexpr uint16_t kSoundQuietPeakToPeak = 5;
constexpr uint16_t kSoundLoudPeakToPeak = 250;

constexpr char kTimeApiBase[] =
    "https://timeapi.io/api/Time/current/zone?timeZone=";

struct TimeZoneOption {
    const char* label;
    const char* apiName;
};

const TimeZoneOption kTimeZones[] = {
    {"Eastern Time - New York", "America/New_York"},
    {"Central Time - Chicago", "America/Chicago"},
    {"Mountain Time - Denver", "America/Denver"},
    {"Pacific Time - Los Angeles", "America/Los_Angeles"},
    {"Alaska Time - Anchorage", "America/Anchorage"},
    {"Hawaii-Aleutian Time - Honolulu", "Pacific/Honolulu"},
    {"Atlantic Time - San Juan", "America/Puerto_Rico"},
};

constexpr size_t kTimeZoneCount =
    sizeof(kTimeZones) / sizeof(kTimeZones[0]);

String base64Encode(const String& input) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    String output;
    output.reserve(((input.length() + 2) / 3) * 4);

    for (size_t index = 0; index < input.length(); index += 3) {
        const uint32_t first =
            static_cast<uint8_t>(input[index]);
        const uint32_t second =
            index + 1 < input.length()
                ? static_cast<uint8_t>(input[index + 1])
                : 0;
        const uint32_t third =
            index + 2 < input.length()
                ? static_cast<uint8_t>(input[index + 2])
                : 0;
        const uint32_t triplet =
            (first << 16) | (second << 8) | third;

        output += alphabet[(triplet >> 18) & 0x3F];
        output += alphabet[(triplet >> 12) & 0x3F];
        output += index + 1 < input.length()
                      ? alphabet[(triplet >> 6) & 0x3F]
                      : '=';
        output += index + 2 < input.length()
                      ? alphabet[triplet & 0x3F]
                      : '=';
    }

    return output;
}

String percentEncode(const String& input) {
    static const char hex[] = "0123456789ABCDEF";
    String output;
    output.reserve(input.length() * 3);

    for (size_t index = 0; index < input.length(); ++index) {
        const uint8_t value = static_cast<uint8_t>(input[index]);
        const bool unreserved =
            (value >= 'A' && value <= 'Z') ||
            (value >= 'a' && value <= 'z') ||
            (value >= '0' && value <= '9') ||
            value == '-' || value == '_' || value == '.' || value == '~';

        if (unreserved) {
            output += static_cast<char>(value);
        } else {
            output += '%';
            output += hex[(value >> 4) & 0x0F];
            output += hex[value & 0x0F];
        }
    }

    return output;
}

// The TimeAPI response gives wall-clock time for the selected zone. Treat that
// calendar value as UTC internally so gmtime_r() later returns the same wall time.
time_t wallTimeToEpoch(int year, int month, int day,
                       int hour, int minute, int second) {
    year -= (month <= 2);
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra =
        static_cast<unsigned>(year - era * 400);
    const unsigned adjustedMonth =
        static_cast<unsigned>(month + (month > 2 ? -3 : 9));
    const unsigned dayOfYear =
        (153 * adjustedMonth + 2) / 5 +
        static_cast<unsigned>(day - 1);
    const unsigned dayOfEra =
        yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 +
        dayOfYear;
    const int64_t days =
        static_cast<int64_t>(era) * 146097 + dayOfEra - 719468;

    return static_cast<time_t>(
        days * 86400 + hour * 3600 + minute * 60 + second);
}

}  // namespace

SensorProject::SensorProject()
    : oneWire_(D2),
      temperatureSensor_(&oneWire_),
      button_{D5, false, false, 0},
      tiltSwitch_{D6, false, false, 0} {
    strncpy(selectedTimeZone_, "America/Los_Angeles",
            sizeof(selectedTimeZone_) - 1);
    selectedTimeZone_[sizeof(selectedTimeZone_) - 1] = '\0';
}

void SensorProject::begin() {
    Serial.println();
    Serial.println(F("ESP8266 temperature and sound logger starting."));

    pinMode(button_.pin, INPUT_PULLUP);
    pinMode(tiltSwitch_.pin, INPUT_PULLUP);
    pinMode(kSoundPin, INPUT);

    const uint32_t now = millis();
    button_.lastRawActive = digitalRead(button_.pin) == LOW;
    button_.stableActive = button_.lastRawActive;
    button_.lastChangeMs = now;
    tiltSwitch_.lastRawActive = digitalRead(tiltSwitch_.pin) == LOW;
    tiltSwitch_.stableActive = tiltSwitch_.lastRawActive;
    tiltSwitch_.lastChangeMs = now;

    temperatureSensor_.begin();
    temperatureSensor_.setResolution(10);
    Serial.printf("[DS18B20] Found %u sensor(s).\n",
                  temperatureSensor_.getDeviceCount());

    selectTimeZone();

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    Serial.println(F("[WiFi] Connecting..."));
    WiFi.begin(WIFI_SSID_VALUE, WIFI_PASSWORD_VALUE);
    lastWiFiAttemptMs_ = millis();
}

void SensorProject::selectTimeZone() {
    Serial.println();
    Serial.println(F("Choose your time zone by entering 1-7 over USB serial:"));
    for (size_t index = 0; index < kTimeZoneCount; ++index) {
        Serial.printf("  %u) %s\n",
                      static_cast<unsigned>(index + 1),
                      kTimeZones[index].label);
    }
    Serial.println(F("Enter a number within 10 seconds. Default: 4, Pacific Time."));

    const uint32_t startedAt = millis();
    while (static_cast<uint32_t>(millis() - startedAt) < kTimeZoneWaitMs) {
        if (Serial.available() > 0) {
            String input = Serial.readStringUntil('\n');
            input.trim();
            const int choice = input.toInt();

            if (choice >= 1 && choice <= static_cast<int>(kTimeZoneCount)) {
                const char* zone = kTimeZones[choice - 1].apiName;
                strncpy(selectedTimeZone_, zone,
                        sizeof(selectedTimeZone_) - 1);
                selectedTimeZone_[sizeof(selectedTimeZone_) - 1] = '\0';
                Serial.printf("[Time] Selected %s.\n",
                              kTimeZones[choice - 1].label);
                return;
            }

            Serial.println(F("[Time] Invalid choice; enter a number from 1 to 7."));
        }
        delay(10);
    }

    Serial.println(F("[Time] No selection; using Pacific Time."));
}

void SensorProject::update() {
    updateWiFi();

    // Poll the switches before any network request so normal loop operation
    // detects each debounced press/release edge.
    if (switchActivated(button_)) {
        captureAndQueue("node_1");
    }
    if (switchActivated(tiltSwitch_)) {
        captureAndQueue("node_2");
    }

    updateClock();
    sendPendingMeasurements();
}

void SensorProject::updateWiFi() {
    const uint32_t now = millis();
    const bool connected = WiFi.status() == WL_CONNECTED;

    if (connected) {
        if (!wasConnected_) {
            wasConnected_ = true;
            printConnectionInfo();
            lastRssiPrintMs_ = now;
            // Try the first time synchronization immediately after connecting.
            lastClockAttemptMs_ = now - kClockRetryMs;
        }

        if (static_cast<uint32_t>(now - lastRssiPrintMs_) >= kRssiPrintMs) {
            Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
            lastRssiPrintMs_ = now;
        }
        return;
    }

    if (wasConnected_) {
        wasConnected_ = false;
        Serial.println(F("[WiFi] Connection lost; reconnecting."));
    }

    if (static_cast<uint32_t>(now - lastWiFiAttemptMs_) >= kWiFiRetryMs) {
        Serial.println(F("[WiFi] Retrying connection..."));
        WiFi.begin(WIFI_SSID_VALUE, WIFI_PASSWORD_VALUE);
        lastWiFiAttemptMs_ = now;
    }
}

void SensorProject::updateClock() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    const uint32_t now = millis();
    const bool firstSyncRetry =
        !clockReady_ &&
        static_cast<uint32_t>(now - lastClockAttemptMs_) >= kClockRetryMs;
    const bool refresh =
        clockReady_ &&
        static_cast<uint32_t>(now - lastClockSyncMs_) >= kClockSyncIntervalMs;

    if (firstSyncRetry || refresh) {
        lastClockAttemptMs_ = now;
        if (synchronizeClock()) {
            clockReady_ = true;
            lastClockSyncMs_ = millis();
            Serial.println(F("[Time] Clock synchronized."));
        }
    }
}

bool SensorProject::synchronizeClock() {
    const String url = String(kTimeApiBase) + selectedTimeZone_;

    BearSSL::WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(secureClient, url)) {
        Serial.println(F("[Time] Could not start the HTTPS request."));
        return false;
    }

    const int status = http.GET();
    if (status < 200 || status >= 300) {
        Serial.printf("[Time] TimeAPI returned HTTP %d.\n", status);
        http.end();
        return false;
    }

    const String body = http.getString();
    http.end();

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, body);
    if (error) {
        Serial.printf("[Time] Could not parse TimeAPI JSON: %s\n",
                      error.c_str());
        return false;
    }

    const char* dateTime = document["dateTime"] | "";
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (sscanf(dateTime, "%4d-%2d-%2dT%2d:%2d:%2d",
               &year, &month, &day, &hour, &minute, &second) != 6) {
        Serial.println(F("[Time] TimeAPI did not return a valid dateTime."));
        return false;
    }

    const time_t epoch =
        wallTimeToEpoch(year, month, day, hour, minute, second);
    if (epoch <= 0) {
        Serial.println(F("[Time] Invalid date returned by TimeAPI."));
        return false;
    }

    struct timeval currentTime {};
    currentTime.tv_sec = epoch;
    currentTime.tv_usec = 0;
    if (settimeofday(&currentTime, nullptr) != 0) {
        Serial.println(F("[Time] Could not set the ESP clock."));
        return false;
    }

    Serial.printf("[Time] Using %s wall-clock time.\n", selectedTimeZone_);
    return true;
}

bool SensorProject::switchActivated(SwitchInput& input) {
    const bool rawActive = digitalRead(input.pin) == LOW;
    const uint32_t now = millis();

    if (rawActive != input.lastRawActive) {
        input.lastRawActive = rawActive;
        input.lastChangeMs = now;
    }

    if (rawActive != input.stableActive &&
        static_cast<uint32_t>(now - input.lastChangeMs) >= kSwitchDebounceMs) {
        input.stableActive = rawActive;
        if (input.stableActive) {
            Serial.printf("[Input] Pin D%u activated.\n",
                          input.pin == D5 ? 5 : 6);
            return true;
        }
    }

    return false;
}

void SensorProject::captureAndQueue(const char* nodeName) {
    if (!clockReady_) {
        Serial.println(F("[Measurement] Waiting for Wi-Fi and time synchronization."));
        return;
    }

    if (pendingCount_ >= kMaxPending) {
        Serial.println(F("[Measurement] Retry queue is full; this activation was not queued."));
        return;
    }

    temperatureSensor_.requestTemperatures();
    const float temperatureC = temperatureSensor_.getTempCByIndex(0);
    if (!isfinite(temperatureC) ||
        temperatureC == DEVICE_DISCONNECTED_C ||
        temperatureC < -10.0f || temperatureC > 100.0f) {
        Serial.println(F("[Measurement] DS18B20 value is unavailable or outside -10 to 100 C."));
        return;
    }

    const uint8_t soundPercent = readSoundPercent();

    Measurement& measurement = pending_[pendingCount_];
    memset(&measurement, 0, sizeof(measurement));
    strncpy(measurement.nodeName, nodeName,
            sizeof(measurement.nodeName) - 1);
    measurement.temperatureC = temperatureC;
    measurement.soundPercent = soundPercent;

    if (!makeTimestamp(measurement.timestamp, sizeof(measurement.timestamp))) {
        Serial.println(F("[Measurement] No valid timestamp; measurement was not queued."));
        return;
    }

    ++pendingCount_;
    Serial.printf("[Measurement] %s temperature=%.2f C sound=%u%% time=%s\n",
                  measurement.nodeName,
                  measurement.temperatureC,
                  measurement.soundPercent,
                  measurement.timestamp);

    sendPendingMeasurements();
}

uint8_t SensorProject::readSoundPercent() {
    uint16_t minimum = 1023;
    uint16_t maximum = 0;

    for (uint8_t sample = 0; sample < kSoundSampleCount; ++sample) {
        const uint16_t reading =
            static_cast<uint16_t>(analogRead(kSoundPin));
        if (reading < minimum) {
            minimum = reading;
        }
        if (reading > maximum) {
            maximum = reading;
        }
        delayMicroseconds(kSoundSampleDelayUs);
    }

    const uint16_t peakToPeak = maximum - minimum;
    const int32_t range =
        static_cast<int32_t>(kSoundLoudPeakToPeak) -
        static_cast<int32_t>(kSoundQuietPeakToPeak);
    int32_t percent =
        (static_cast<int32_t>(peakToPeak) -
         static_cast<int32_t>(kSoundQuietPeakToPeak)) * 100 / range;

    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }

    Serial.printf("[Sound] AO peak-to-peak=%u, relative activity=%ld%%\n",
                  peakToPeak, static_cast<long>(percent));
    return static_cast<uint8_t>(percent);
}

bool SensorProject::makeTimestamp(char* output, size_t outputSize) const {
    if (!clockReady_ || outputSize < 20) {
        return false;
    }

    const time_t now = time(nullptr);
    struct tm calendarTime {};
    if (gmtime_r(&now, &calendarTime) == nullptr) {
        return false;
    }

    return strftime(output, outputSize, "%Y-%m-%d %H:%M:%S",
                    &calendarTime) > 0;
}

void SensorProject::sendPendingMeasurements() {
    if (pendingCount_ == 0 || WiFi.status() != WL_CONNECTED) {
        return;
    }

    Measurement& measurement = pending_[0];
    const uint32_t now = millis();
    if (measurement.attempted &&
        static_cast<uint32_t>(now - measurement.lastAttemptMs) <
            kMessageRetryMs) {
        return;
    }

    measurement.attempted = true;
    measurement.lastAttemptMs = now;

    if (!sendMeasurement(measurement)) {
        Serial.println(F("[Upload] Not accepted; the same reading will retry in 30 seconds."));
        return;
    }

    Serial.println(F("[Upload] Accepted or already stored; removing from retry queue."));
    for (uint8_t index = 1; index < pendingCount_; ++index) {
        pending_[index - 1] = pending_[index];
    }
    --pendingCount_;
}

bool SensorProject::sendMeasurement(const Measurement& measurement) {
    String payload;
    payload.reserve(128);
    payload += "nodeId=";
    payload += measurement.nodeName;
    payload += "&nodeTemp=";
    payload += String(measurement.temperatureC, 2);
    // The current database and PHP endpoint use the legacy humidity field for
    // the sound module's 0-100 relative activity reading.
    payload += "&nodeHumidity=";
    payload += String(measurement.soundPercent);
    payload += "&timeReceived=";
    payload += percentEncode(String(measurement.timestamp));

    const String query = percentEncode(base64Encode(payload));
    const String url = String(UPLOAD_URL_VALUE) + "?" + query;

    BearSSL::WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(secureClient, url)) {
        Serial.println(F("[Upload] Could not start the HTTPS request."));
        return false;
    }

    const int status = http.GET();
    const String response = status > 0 ? http.getString() : String();
    http.end();

    Serial.printf("[Upload] HTTP status: %d\n", status);
    if (response.length() > 0) {
        Serial.printf("[Upload] Server: %s\n", response.c_str());
    }

    // 409 is the server's response for this node/timestamp already being stored.
    return status == 200 || status == 201 || status == 409;
}

void SensorProject::printConnectionInfo() const {
    Serial.println(F("[WiFi] Connected."));
    Serial.print(F("[WiFi] IP address: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("[WiFi] MAC address: "));
    Serial.println(WiFi.macAddress());
    Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
}

