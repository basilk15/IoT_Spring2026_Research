# IoT Spring 2026 Research

This repository collects the code, data, and reports used for an IoT and federated learning research workflow centered on ESP32-based sensing, MPU6050 motion data, and lightweight on-device training experiments.

The repo is no longer just a pair of Arduino sketches. It now contains:

- baseline ESP32 federated learning client/server code
- a cycle-measurement variant of the FL baseline
- a FreeRTOS-based FL task implementation with CPU usage reporting
- Zephyr sensor and threading examples for ESP32
- an exported Edge Impulse C++ library and sample datasets
- generated reports, notes, and diagrams used during analysis

## Repository Overview

### Main experiment folders

- `FL_baseline_implementation/`
  Baseline Arduino + Python federated learning setup.
  Includes:
  - `FL_client_esp32.ino`: ESP32 client that reads MPU6050 data, downloads model weights, trains locally, and uploads updates
  - `FL_Server.py`: Flask aggregation server with federated averaging
  - `CC2650_Blue_reader.ino`: BLE-based SensorTag reader
  - `FL_Project_Documentation (1).docx`: supporting project documentation

- `FL_baseline_implementation_cycles_measured/`
  Measurement-focused version of the baseline FL workflow.
  Includes:
  - `FL_client_esp32/FL_client_esp32.ino`: client variant used for resource/cycle measurements
  - `FL_Server.py`: baseline server copy
  - `FL_Server_freertos_measurement.py`: measurement-oriented server variant with `MIN_CLIENTS = 1` for single-device runs
  - `stats.md`: recorded flash and RAM usage notes

- `FL_freertos_task/`
  FreeRTOS-based ESP32 FL client implementation.
  Includes:
  - `FL_freertos_task.ino`: moves one federated learning round into a dedicated `FLTask` and captures FreeRTOS run-time statistics before and after execution

- `Zpehyr_Application/`
  Zephyr sample applications kept alongside the Arduino experiments.
  Includes:
  - `mpu6050/`: Zephyr MPU6050 sample for ESP32 DevKitC
  - `threads/`: Zephyr threading sample

- `aa08453-project-1-cpp-linux-v6/`
  Exported Edge Impulse C++ inferencing library for the project model, including SDK, generated model files, and a Linux example entry point.

- `Samples_edgeimpulse/`
  Sample JSON files for motion classes and test inputs used around the Edge Impulse workflow.

### Top-level sketches, reports, and utilities

- `cc2650_w_esp32.ino`
  ESP32 BLE client for reading TI CC2650 SensorTag temperature data.

- `mputest_18feb/mputest_18feb.ino`
  Standalone MPU6050 test sketch.

- `free_rtos_measure_example.ino`
  Smaller FreeRTOS measurement example sketch.

- `generate_fl_freertos_report.py`
  Python script that generates the PDF report summarizing the FreeRTOS-based CPU usage measurement approach.

- `FL_freertos_cpu_measurement_report.pdf`
- `FL_freertos_cpu_measurement_report_prev.pdf`
- `FL_freertos_cpu_measurement_report_prev2.pdf`
- `FL_freertos_cpu_measurement_report_prev3.pdf`
- `fl_round_cost_analysis.pdf`
- `federated learning cycles.txt`
- `FL_freertos_task_flow.excalidraw`
  Analysis artifacts and generated reports for the research write-up.

## What Each Track Covers

### 1. Baseline federated learning

The baseline implementation combines:

- an ESP32 client that reads sensor values from an MPU6050 over I2C
- a small neural network with 9 input features and 332 total parameters
- a Flask server that shares global weights and aggregates client updates

This is the main reference implementation if you want to understand the full ESP32-to-server FL flow first.

### 2. Cycle and memory measurement

The cycle-measured variant keeps the same overall FL workflow but adds measurement-oriented supporting files and notes. This path is useful when comparing the computational cost of local training and communication on the ESP32.

### 3. FreeRTOS task-based measurement

The `FL_freertos_task` sketch isolates the FL round inside a dedicated FreeRTOS task and compares system task run-time snapshots before and after execution. This gives a more realistic picture of CPU usage than a simple elapsed cycle count around a code block.

### 4. Zephyr experiments

The Zephyr samples are separate from the Arduino FL path and appear to be used for exploring:

- MPU6050 integration under Zephyr
- thread behavior on ESP32 with Zephyr APIs

### 5. Edge Impulse model export

The Edge Impulse export contains the generated C++ inference library and model assets for local inference experiments outside the Arduino FL client.

## Suggested Starting Points

If you want to work on a specific part of the repo, these are the best entry points:

- Federated learning client: `FL_baseline_implementation/FL_client_esp32.ino`
- Federated learning server: `FL_baseline_implementation/FL_Server.py`
- FreeRTOS measurement client: `FL_freertos_task/FL_freertos_task.ino`
- Single-device measurement server: `FL_baseline_implementation_cycles_measured/FL_Server_freertos_measurement.py`
- Edge Impulse Linux example: `aa08453-project-1-cpp-linux-v6/main.cpp`
- Zephyr sensor example: `Zpehyr_Application/mpu6050/src/main.c`

## Requirements

Requirements depend on which part of the repo you want to run.

### Arduino / ESP32 work

- Arduino IDE or Arduino CLI
- ESP32 board support package
- I2C-connected MPU6050 for the FL and sensor sketches
- Optional TI CC2650 SensorTag for the BLE temperature sketches

Common Arduino libraries and components used in this repo include:

- `WiFi.h`
- `HTTPClient.h`
- `Wire.h`
- ESP32 BLE headers such as `BLEDevice`, `BLEClient`, and `BLEUtils`
- `freertos/FreeRTOS.h` and `freertos/task.h` for the FreeRTOS path
- `Adafruit_MPU6050`
- `Adafruit Unified Sensor`

### Python tools

For the Flask servers and report generation scripts:

- Python 3
- `flask`
- `numpy`
- `reportlab`

Example install:

```bash
pip install flask numpy reportlab
```

### Zephyr work

- Zephyr development environment
- an ESP32 board target such as `esp32_devkitc/esp32/procpu`
- any board overlay needed for the MPU6050 wiring

### Edge Impulse Linux example

Inside `aa08453-project-1-cpp-linux-v6/`:

```bash
make -j4
./build/app
```

## Running the Main Components

### Baseline FL server

From `FL_baseline_implementation/`:

```bash
python3 FL_Server.py
```

The server exposes:

- `GET /model`
- `POST /update`
- `GET /status`

### Measurement FL server

From `FL_baseline_implementation_cycles_measured/`:

```bash
python3 FL_Server_freertos_measurement.py
```

### ESP32 clients

1. Open the required sketch in Arduino IDE.
2. Update Wi-Fi credentials, server IP, and any sensor-specific settings in the sketch.
3. Select the correct ESP32 board and serial port.
4. Upload the sketch and open Serial Monitor at `115200` baud unless changed in code.

### Generate the FreeRTOS CPU report

From the repository root:

```bash
python3 generate_fl_freertos_report.py
```

This writes `FL_freertos_cpu_measurement_report.pdf`.

## Notes and Caveats

- Several sketches currently contain hardcoded Wi-Fi credentials, server IPs, and device identifiers. These should be adjusted before running on another setup.
- The repo contains both exploratory prototypes and report artifacts, so not every file is part of one single build pipeline.
- The folder name `Zpehyr_Application/` is kept as-is to match the current repository structure.
- The Edge Impulse export includes a large vendor/generated tree; treat it as generated support code unless you specifically need to modify the inference path.

## Current High-Level Structure

```text
.
|-- FL_baseline_implementation/
|-- FL_baseline_implementation_cycles_measured/
|-- FL_freertos_task/
|-- Samples_edgeimpulse/
|-- Zpehyr_Application/
|-- aa08453-project-1-cpp-linux-v6/
|-- mputest_18feb/
|-- cc2650_w_esp32.ino
|-- free_rtos_measure_example.ino
|-- generate_fl_freertos_report.py
|-- README.md
```
