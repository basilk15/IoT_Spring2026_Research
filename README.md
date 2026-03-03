# IoT Spring 2026 Research

Arduino-based sensor prototypes for:
- BLE temperature reads from a TI CC2650 SensorTag (using ESP32 as client)
- MPU6050 accelerometer/gyroscope streaming over serial

## Repository Layout

- `cc2650_w_esp32.ino`  
  ESP32 BLE client that connects to a fixed SensorTag MAC address and prints ambient/object temperature every second.
- `mputest_18feb/mputest_18feb.ino`  
  MPU6050 test sketch using Adafruit libraries; prints acceleration and gyro values every 500 ms.

## Requirements

- Arduino IDE (or Arduino CLI)
- ESP32 board package
- Libraries:
  - `Adafruit_MPU6050`
  - `Adafruit Unified Sensor`
  - `Wire` (built-in)
  - ESP32 BLE headers (`BLEDevice`, `BLEClient`, `BLEUtils`) from ESP32 core

## Usage

1. Open one sketch at a time in Arduino IDE.
2. Select board and port.
3. Install missing libraries from Library Manager.
4. Upload and open Serial Monitor at `115200` baud.


For running the edge impulse model, navigate to the cpp library folder. There:

run ```make j -4```

then ```build/app```

## Configuration Notes

### CC2650 + ESP32 (`cc2650_w_esp32.ino`)
- Set `SENSORTAG_ADDRESS` to your SensorTag BLE MAC.
- Uses TI temperature service UUIDs and writes `0x01` to enable temperature sensing.
- Auto-reconnects if the SensorTag disconnects.

### MPU6050 (`mputest_18feb/mputest_18feb.ino`)
- Expects MPU6050 available on I2C.
- Current settings in code:
  - Accelerometer range: `+-8G`
  - Gyro range: `+-500 deg/s`
  - Filter bandwidth: `5 Hz`


