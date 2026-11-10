/*
 * Project/Program Name : ESP8266 Temperature and Sound Logger
 * File Name            : ProjectFunctions.h
 * Author               : Yahir Magana
 * Date                 : 2026-11-06
 * Version              : 2.0
 *
 * Purpose:
 * Declares the single project controller used by the short main loop.
 *
 * Inputs:
 * DS18B20, sound sensor, pushbutton, tilt sensor, Wi-Fi, and USB serial input.
 *
 * Outputs:
 * Timestamped sensor readings sent to the HTTPS PHP endpoint and status output.
 *
 * Example Application:
 * Call begin() once from setup() and update() repeatedly from loop().
 *
 * Dependencies:
 * Arduino framework, OneWire, and DallasTemperature.
 *
 * Usage Notes:
 * Pin assignments are for a NodeMCU ESP8266 development board.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

class SensorProject {
public:
    SensorProject();

    void begin();
    void update();

private:
    static constexpr uint8_t kMaxPending = 5;

    struct SwitchInput {
        uint8_t pin;
        bool lastRawActive;
        bool stableActive;
        uint32_t lastChangeMs;
    };

    struct Measurement {
        char nodeName[11];
        char timestamp[20];
        float temperatureC;
        uint8_t soundPercent;
        bool attempted;
        uint32_t lastAttemptMs;
    };

    void selectTimeZone();
    void updateWiFi();
    void updateClock();
    bool synchronizeClock();
    bool switchActivated(SwitchInput& input);
    void captureAndQueue(const char* nodeName);
    uint8_t readSoundPercent();
    bool makeTimestamp(char* output, size_t outputSize) const;
    void sendPendingMeasurements();
    bool sendMeasurement(const Measurement& measurement);
    void printConnectionInfo() const;

    OneWire oneWire_;
    DallasTemperature temperatureSensor_;

    SwitchInput button_;
    SwitchInput tiltSwitch_;

    Measurement pending_[kMaxPending];
    uint8_t pendingCount_ = 0;

    char selectedTimeZone_[32];
    bool wasConnected_ = false;
    bool clockReady_ = false;

    uint32_t lastWiFiAttemptMs_ = 0;
    uint32_t lastClockAttemptMs_ = 0;
    uint32_t lastClockSyncMs_ = 0;
    uint32_t lastRssiPrintMs_ = 0;
};

