#-----------------------------------
# Title: Serial Data to CSV Logger
#-----------------------------------
# Program Detail:
#-----------------------------------
# Purpose: Reads serial data from a COM port and logs it to a CSV file.
# Inputs: Serial data from ESP8266 (via COM3 at 9600 baud)
# Outputs: Logged data into ChargingVoltage.csv
# Date: October 5, 2026
# Compiler: Python 3
# Author: Yahir Magana
# Versions:
#       V1 - Initial version capturing serial data and saving it to a CSV file.
#-----------------------------------
# File Dependencies: these are the listing and header files you need to run the
#   program
#-----------------------------------

import serial
import csv

#-----------------------------------
# Main Program
#-----------------------------------

COM_PORT = 'COM3'
BAUD_RATE = 9600
CSV_FILENAME = 'ChargingVoltage.csv'

try:
    ser = serial.Serial(COM_PORT, BAUD_RATE)
    print(f"Connected to {COM_PORT}. Listening for data...")

    with open(CSV_FILENAME, mode='a', newline='') as file:
        writer = csv.writer(file)

        while True:
            if ser.in_waiting > 0:
                # Adding errors='ignore' prevents crashes from bootloader garbage
                line = ser.readline().decode('utf-8', errors='ignore').strip()

                # Only process the line if it actually contains data
                if line:
                    print(f"Received: {line}")
                    data = line.split(',')
                    writer.writerow(data)
                    file.flush()

except KeyboardInterrupt:
    print("\nData collection stopped.")
except Exception as e:
    print(f"Error: {e}")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
