#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>

// CC2650 SensorTag Temperature Service & Characteristics
#define TEMP_SERVICE_UUID        "f000aa00-0451-4000-b000-000000000000"
#define TEMP_DATA_UUID           "f000aa01-0451-4000-b000-000000000000"
#define TEMP_CONFIG_UUID         "f000aa02-0451-4000-b000-000000000000"
#define TEMP_PERIOD_UUID         "f000aa03-0451-4000-b000-000000000000"

// Replace with your SensorTag's address
#define SENSORTAG_ADDRESS        "b0:91:22:f6:c5:87"

BLEClient* pClient;
BLERemoteCharacteristic* pTempData;
BLERemoteCharacteristic* pTempConfig;

bool connected = false;

// ---- Callback: handle disconnect ----
class ClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    connected = true;
    Serial.println("Connected to SensorTag!");
  }
  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("Disconnected from SensorTag.");
  }
};

// ---- Convert raw bytes to Celsius ----
// CC2650 returns two 16-bit values: [object temp (IR), ambient temp]
// We'll read the ambient (die) temperature
float parseTemperature(uint8_t* data) {
  // Ambient temp is in bytes [2] and [3]
  int16_t rawAmbient = (int16_t)((data[3] << 8) | data[2]);
  float ambientC = rawAmbient / 128.0;
  return ambientC;
}

float parseIRTemperature(uint8_t* data) {
  // Object (IR) temp is in bytes [0] and [1]
  int16_t rawObject = (int16_t)((data[1] << 8) | data[0]);
  float objectC = rawObject / 128.0;
  return objectC;
}

bool connectAndSetup() {
  BLEAddress sensorAddress(SENSORTAG_ADDRESS);

  Serial.println("Connecting to SensorTag...");
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new ClientCallbacks());

  if (!pClient->connect(sensorAddress)) {
    Serial.println("Failed to connect!");
    return false;
  }

  // Get temperature service
  BLERemoteService* pTempService = pClient->getService(TEMP_SERVICE_UUID);
  if (!pTempService) {
    Serial.println("Temperature service not found!");
    pClient->disconnect();
    return false;
  }

  // Get characteristics
  pTempData   = pTempService->getCharacteristic(TEMP_DATA_UUID);
  pTempConfig = pTempService->getCharacteristic(TEMP_CONFIG_UUID);

  if (!pTempData || !pTempConfig) {
    Serial.println("Temperature characteristics not found!");
    pClient->disconnect();
    return false;
  }

  // Enable the temperature sensor (write 0x01 to config)
  uint8_t enable = 0x01;
  pTempConfig->writeValue(&enable, 1);
  Serial.println("Temperature sensor enabled.");

  delay(300); // Give sensor time to warm up

  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("CC2650 SensorTag Temperature Reader");

  BLEDevice::init("ESP32-SensorTag-Client");

  if (connectAndSetup()) {
    Serial.println("Setup complete. Reading temperature...");
  } else {
    Serial.println("Setup failed. Restarting in 5s...");
    delay(5000);
    ESP.restart();
  }
}

void loop() {
  if (!connected) {
    Serial.println("Reconnecting...");
    if (connectAndSetup()) {
      Serial.println("Reconnected!");
    } else {
      delay(5000);
      return;
    }
  }

  // Read raw temperature data (4 bytes)
  String rawData = pTempData->readValue();  // Arduino String in core 3.x

  if (rawData.length() >= 4) {
    uint8_t* data = (uint8_t*)rawData.c_str();

    float ambientC = parseTemperature(data);
    float objectC  = parseIRTemperature(data);
    float ambientF = (ambientC * 9.0 / 5.0) + 32;
    float objectF  = (objectC  * 9.0 / 5.0) + 32;

    Serial.println("---------------------------");
    Serial.printf("Ambient (Die): %.2f C / %.2f F\n", ambientC, ambientF);
    Serial.printf("Object  (IR):  %.2f C / %.2f F\n", objectC,  objectF);
  } else {
    Serial.println("Failed to read temperature data.");
  }

  delay(1000);
}
