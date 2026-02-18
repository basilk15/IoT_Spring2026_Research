# IoT Spring 2026 Research

Small Arduino/ESP32 experiments for BLE and IMU sensor data collection.

## Contents

- `cc2650_w_esp32.ino`: ESP32 BLE client that connects to a TI CC2650 SensorTag and reads ambient/object temperature once per second.
- `mputest_18feb/mputest_18feb.ino`: MPU6050 test sketch (Adafruit library) that prints acceleration and gyro readings over serial.

## Hardware

- ESP32 development board
- TI CC2650 SensorTag (for BLE temperature sketch)
- MPU6050 IMU module (for IMU sketch)

## Arduino Dependencies

- ESP32 core BLE libraries (`BLEDevice`, `BLEClient`, `BLEUtils`)
- `Adafruit_MPU6050`
- `Adafruit Unified Sensor`
- `Wire`

## Quick Start

1. Open the target `.ino` sketch in Arduino IDE.
2. Install required libraries from Library Manager.
3. Select your board/port.
4. Upload and open Serial Monitor at `115200` baud.

## Sketch Notes

### `cc2650_w_esp32.ino`

- Update `SENSORTAG_ADDRESS` to your SensorTag MAC address before upload.
- Uses SensorTag temperature service UUIDs and enables sensor config by writing `0x01`.
- Prints both ambient (die) and IR object temperature in Celsius and Fahrenheit.

### `mputest_18feb/mputest_18feb.ino`

- Initializes MPU6050, sets accel range to `+-8G`, gyro to `+-500 deg/s`, bandwidth to `5 Hz`.
- Prints acceleration (`m/s^2`) and rotation (`rad/s`) every 500 ms.
