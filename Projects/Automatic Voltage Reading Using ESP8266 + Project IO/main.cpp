//-----------------------------------
// Title: Battery Voltage Logger
//-----------------------------------
// Program Detail:
//-----------------------------------
// Purpose: Reads a voltage at pin A0 every 60 seconds and outputs CSV data over Serial.
// Inputs: Analog pin A0 (from voltage divider)
// Outputs: Serial output (Elapsed Time in ms, Raw ADC, Battery Voltage in V)
// Date: October 5, 2026
// Compiler: VS Code + Project IO
// Author: Yahir Magana
// Versions:
//       V1 - Initial version reading voltage and outputting to Serial over 60s intervals.
//
//-----------------------------------
// File Dependencies: these are the listing and header files you need to run the
//   program
//-----------------------------------

#include <Arduino.h>

//-----------------------------------
// Main Program
//-----------------------------------

const int analogPin = A0;
const unsigned long interval = 60000; // 60 seconds
unsigned long previousMillis = 0;

// Voltage divider resistors for a max microcontroller pin voltage of 3.3V 
const float R1 = 10000.0; // 10k Ohm
const float R2 = 3300.0; // 3.3k Ohm

void setup() {
  // Start the serial communication at 9600 baud rate
  Serial.begin(9600);
  
  delay(1000);
  Serial.println("Elapsed Time(ms),Raw ADC,Battery Voltage(V)");
}

void loop() {
  unsigned long currentMillis = millis();

  // Measurement once every minute (60 seconds)
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Raw ADC value (0-1023)
    int rawADC = analogRead(analogPin);
    
    // Reading up to 3.3V natively:
    float pinVoltage = rawADC * (3.3 / 1023.0); 

    // Convert the ADC reading to the actual battery voltage using the voltage-divider relationship
    float batteryVoltage = pinVoltage / (R1 / (R2 + R1));

    // Send the data formatted as a CSV row: "ElapsedTime,RawADC,BatteryVoltage"
    Serial.print(currentMillis); 
    Serial.print(",");
    Serial.print(rawADC);
    Serial.print(",");
    Serial.println(batteryVoltage);
  }
}
