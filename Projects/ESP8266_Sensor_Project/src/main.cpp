/*
 * Project/Program Name : ESP8266 Temperature and Sound Logger
 * File Name            : main.cpp
 * Author               : Yahir Magana
 * Date                 : 2026-11-06
 * Version              : 2.0
 *
 * Purpose:
 * Starts the sensor project and runs its regular update function.
 *
 * Inputs:
 * SensorProject hardware inputs and USB serial time-zone selection.
 *
 * Outputs:
 * Setup/status output through USB serial; sensor data is sent by the project.
 *
 * Example Application:
 * setup() initializes the ESP8266; loop() repeatedly checks switches and retries
 * any sensor message that the server has not accepted.
 *
 * Dependencies:
 * ProjectFunctions.h and the ESP8266 Arduino framework.
 *
 * Usage Notes:
 * Keep loop() short; component and network work is implemented in
 * ProjectFunctions.cpp.
 * ---------------------------------------------------------------------------
 */

#include <Arduino.h>
#include "ProjectFunctions.h"

SensorProject project;

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(50);
    delay(100);
    project.begin();
}

void loop() {
    project.update();
    delay(2);
}

