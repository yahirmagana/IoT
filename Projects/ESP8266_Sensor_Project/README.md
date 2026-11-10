# ESP8266 Temperature and Sound Logger

This is a simplified PlatformIO project for a NodeMCU development board with an
ESP-12E/ESP8266. The ESP reads a DS18B20 and the kit's big sound module, then
sends measurements only after a pushbutton or tilt-switch activation.

## Files

- `src/main.cpp` contains only initialization and the short main loop.
- `include/ProjectFunctions.h` declares one project class.
- `src/ProjectFunctions.cpp` contains sensor reads, switch debounce, timezone
  choice, Wi-Fi, HTTPS, and retries.
- `server/sensor_data_insert.php` validates and stores a reading.
- `server/create_sensor_activity.sql` creates the activity-counter table.

## Libraries

PlatformIO installs these component/JSON libraries from `platformio.ini`:

- **OneWire** and **DallasTemperature** for the DS18B20.
- **ArduinoJson** for the TimeAPI JSON response.

HTTPS uses `ESP8266HTTPClient.h`, `ESP8266WiFi.h`, and
`WiFiClientSecureBearSSL.h), which are included with the ESP8266 Arduino
platform; they do not need separate `lib_deps` entries.

## Setup

1. Open the `ESP8266_Sensor_Project` folder in VS Code with PlatformIO.
2. Copy `include/Secrets.h.example` to `include/Secrets.h`.
3. Put your Wi-Fi name, Wi-Fi password, and full **HTTPS** endpoint URL in
   `include/Secrets.h`.
4. Upload to a NodeMCU ESP8266 board and open the serial monitor at 115200 baud.
5. Enter a timezone number from 1 to 7 within 10 seconds. If you do not enter a
   number, the code selects Pacific Time.

## Wiring

| Component | NodeMCU pin |
| --- | --- |
| DS18B20 VDD | 3V3 |
| DS18B20 GND | GND |
| DS18B20 DATA | D2 / GPIO4 |
| DS18B20 pull-up | 4.7 kΩ from DATA to 3V3 |
| Big sound module AO | A0 |
| Big sound module VCC/GND | 3V3/GND, if the module operates at 3.3 V |
| Pushbutton contacts | D5 / GPIO14 and GND |
| Tilt switch contacts | D6 / GPIO12 and GND when activated |

The switches use `INPUT_PULLUP`: an open switch reads HIGH, and an activation
connects the input to GND. If your tilt sensor is a powered module with a
digital-output pin, connect its output to D6 and power it at 3.3 V only if its
documentation permits; the code expects an active-LOW output. Do not connect a
5 V output to an ESP8266 GPIO.

The sound module's AO output is sampled at A0. The firmware converts the
peak-to-peak sample range to a relative 0–100 activity percentage; it is not a
calibrated sound-pressure measurement or decibel reading. Adjust
`kSoundQuietPeakToPeak` and `kSoundLoudPeakToPeak` in
`src/ProjectFunctions.cpp` after checking the serial readings. Verify your
NodeMCU board's A0 input range and the module output voltage before wiring.

## Project behavior

- Pushbutton activation sends a measurement as `node_1`.
- Tilt activation sends one measurement as `node_2`.
- A 40 ms debounce makes one event per new activation; holding a switch does
  not repeat a measurement.
- The USB serial menu offers the seven time zones specified by the assignment.
  The firmware gets the selected zone's wall-clock time from TimeAPI and
  refreshes its clock every six hours.
- RSSI, IP address, and MAC address are printed to USB serial.
- A failed upload is retried every 60 seconds. Retries retain the same node,
  timestamp, temperature, and sound value. HTTP 200/201 are accepted, and HTTP
  409 is treated as delivered because that timestamp is already stored.
- The endpoint requires a timestamp and rejects duplicate node/timestamp rows.

The current database/PHP column is named `humidity`; the sound activity
percentage is sent through the legacy `nodeHumidity` field so the existing
table can store the second sensor reading.

## HTTPS note

Both TimeAPI and the data endpoint are called over HTTPS using BearSSL. The
code calls `setInsecure()` to keep classroom setup short. 
