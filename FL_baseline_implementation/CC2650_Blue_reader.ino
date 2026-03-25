#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>

const char* WIFI_SSID   = "pop-os";
const char* WIFI_PASS   = "111222333";
const char* SERVER_IP   = "10.120.60.169";
const int   SERVER_PORT = 5000;
const char* CLIENT_ID   = "esp32-node-B";

static const char* SENSOR_TAG_MAC = "b0:91:22:f6:c5:87";

static BLEUUID movServiceUUID("f000aa80-0451-4000-b000-000000000000");
static BLEUUID movDataUUID   ("f000aa81-0451-4000-b000-000000000000");
static BLEUUID movConfigUUID ("f000aa82-0451-4000-b000-000000000000");
static BLEUUID movPeriodUUID ("f000aa83-0451-4000-b000-000000000000");
static BLEUUID tmpServiceUUID("f000aa00-0451-4000-b000-000000000000");
static BLEUUID tmpDataUUID   ("f000aa01-0451-4000-b000-000000000000");
static BLEUUID tmpConfigUUID ("f000aa02-0451-4000-b000-000000000000");
static BLEUUID tmpPeriodUUID ("f000aa03-0451-4000-b000-000000000000");
static BLEUUID humServiceUUID("f000aa20-0451-4000-b000-000000000000");
static BLEUUID humDataUUID   ("f000aa21-0451-4000-b000-000000000000");
static BLEUUID humConfigUUID ("f000aa22-0451-4000-b000-000000000000");
static BLEUUID humPeriodUUID ("f000aa23-0451-4000-b000-000000000000");
static BLEUUID optServiceUUID("f000aa70-0451-4000-b000-000000000000");
static BLEUUID optDataUUID   ("f000aa71-0451-4000-b000-000000000000");
static BLEUUID optConfigUUID ("f000aa72-0451-4000-b000-000000000000");
static BLEUUID optPeriodUUID ("f000aa73-0451-4000-b000-000000000000");

#define INPUT_DIM     9
#define HIDDEN1      16
#define HIDDEN2       8
#define OUTPUT_DIM    4
#define TOTAL_WEIGHTS 332

float W1[INPUT_DIM][HIDDEN1], b1[HIDDEN1];
float W2[HIDDEN1][HIDDEN2],   b2[HIDDEN2];
float W3[HIDDEN2][OUTPUT_DIM],b3[OUTPUT_DIM];
int current_round = 0;

BLEAdvertisedDevice* foundDevice = nullptr;
bool doConnect = false;

float convertIRTemp(int16_t rawObj) { return rawObj / 128.0f; }
void convertHumidity(uint16_t rawTemp, uint16_t rawHum, float &tempC, float &relHum) {
  tempC  = ((float)rawTemp / 65536.0f) * 165.0f - 40.0f;
  relHum = ((float)rawHum  / 65536.0f) * 100.0f;
}
float convertLux(uint16_t raw) {
  return (raw & 0x0FFF) * (0.01f * (1 << ((raw >> 12) & 0x0F)));
}

void flattenWeights(float* flat) {
  int idx = 0;
  for (int i = 0; i < INPUT_DIM; i++)
    for (int j = 0; j < HIDDEN1; j++) flat[idx++] = W1[i][j];
  for (int j = 0; j < HIDDEN1; j++) flat[idx++] = b1[j];
  for (int i = 0; i < HIDDEN1; i++)
    for (int j = 0; j < HIDDEN2; j++) flat[idx++] = W2[i][j];
  for (int j = 0; j < HIDDEN2; j++) flat[idx++] = b2[j];
  for (int i = 0; i < HIDDEN2; i++)
    for (int j = 0; j < OUTPUT_DIM; j++) flat[idx++] = W3[i][j];
  for (int j = 0; j < OUTPUT_DIM; j++) flat[idx++] = b3[j];
}

void unflattenWeights(float* flat) {
  int idx = 0;
  for (int i = 0; i < INPUT_DIM; i++)
    for (int j = 0; j < HIDDEN1; j++) W1[i][j] = flat[idx++];
  for (int j = 0; j < HIDDEN1; j++) b1[j] = flat[idx++];
  for (int i = 0; i < HIDDEN1; i++)
    for (int j = 0; j < HIDDEN2; j++) W2[i][j] = flat[idx++];
  for (int j = 0; j < HIDDEN2; j++) b2[j] = flat[idx++];
  for (int i = 0; i < HIDDEN2; i++)
    for (int j = 0; j < OUTPUT_DIM; j++) W3[i][j] = flat[idx++];
  for (int j = 0; j < OUTPUT_DIM; j++) b3[j] = flat[idx++];
}

bool parseModelResponse(const String& payload, int& round_out, float* weights_out) {
  int ri = payload.indexOf("\"round\":");
  if (ri < 0) return false;
  round_out = payload.substring(ri + 8).toInt();
  int start = payload.indexOf("\"weights\":[");
  if (start < 0) return false;
  start = payload.indexOf('[', start) + 1;
  int end = payload.indexOf(']', start);
  if (end < 0) return false;
  String arr = payload.substring(start, end);
  int idx = 0, pos = 0;
  while (idx < TOTAL_WEIGHTS && pos < (int)arr.length()) {
    int comma = arr.indexOf(',', pos);
    String token = (comma < 0) ? arr.substring(pos) : arr.substring(pos, comma);
    token.trim();
    weights_out[idx++] = token.toFloat();
    if (comma < 0) break;
    pos = comma + 1;
  }
  return (idx == TOTAL_WEIGHTS);
}

inline float relu(float x) { return x > 0 ? x : 0; }

void forward(const float* x, float* h1, float* h2, float* out) {
  for (int j = 0; j < HIDDEN1; j++) {
    float s = b1[j];
    for (int i = 0; i < INPUT_DIM; i++) s += x[i] * W1[i][j];
    h1[j] = relu(s);
  }
  for (int j = 0; j < HIDDEN2; j++) {
    float s = b2[j];
    for (int i = 0; i < HIDDEN1; i++) s += h1[i] * W2[i][j];
    h2[j] = relu(s);
  }
  for (int j = 0; j < OUTPUT_DIM; j++) {
    float s = b3[j];
    for (int i = 0; i < HIDDEN2; i++) s += h2[i] * W3[i][j];
    out[j] = s;
  }
}

void sgd_step(const float* x, const float* target, float lr = 0.01f) {
  float h1[HIDDEN1], h2[HIDDEN2], out[OUTPUT_DIM];
  forward(x, h1, h2, out);
  float dout[OUTPUT_DIM];
  for (int j = 0; j < OUTPUT_DIM; j++)
    dout[j] = 2.0f * (out[j] - target[j]) / OUTPUT_DIM;
  float dh2[HIDDEN2] = {};
  for (int i = 0; i < HIDDEN2; i++) {
    for (int j = 0; j < OUTPUT_DIM; j++) {
      W3[i][j] -= lr * dout[j] * h2[i];
      dh2[i]   += dout[j] * W3[i][j];
    }
  }
  for (int j = 0; j < OUTPUT_DIM; j++) b3[j] -= lr * dout[j];
  for (int i = 0; i < HIDDEN2; i++) dh2[i] = (h2[i] > 0) ? dh2[i] : 0;
  float dh1[HIDDEN1] = {};
  for (int i = 0; i < HIDDEN1; i++) {
    for (int j = 0; j < HIDDEN2; j++) {
      W2[i][j] -= lr * dh2[j] * h1[i];
      dh1[i]   += dh2[j] * W2[i][j];
    }
  }
  for (int j = 0; j < HIDDEN2; j++) b2[j] -= lr * dh2[j];
  for (int i = 0; i < HIDDEN1; i++) dh1[i] = (h1[i] > 0) ? dh1[i] : 0;
  for (int i = 0; i < INPUT_DIM; i++)
    for (int j = 0; j < HIDDEN1; j++)
      W1[i][j] -= lr * dh1[j] * x[i];
  for (int j = 0; j < HIDDEN1; j++) b1[j] -= lr * dh1[j];
}

void downloadModel() {
  HTTPClient http;
  String url = String("http://") + SERVER_IP + ":" + SERVER_PORT + "/model";
  http.begin(url);
  if (http.GET() != 200) { http.end(); Serial.println("[FL] GET failed"); return; }
  String payload = http.getString();
  http.end();
  float flat[TOTAL_WEIGHTS];
  if (parseModelResponse(payload, current_round, flat)) {
    unflattenWeights(flat);
    Serial.printf("[FL] Model downloaded (round %d)\n", current_round);
  } else {
    Serial.println("[FL] Model parse failed");
  }
}

void uploadWeights(int n_samples) {
  float flat[TOTAL_WEIGHTS];
  flattenWeights(flat);
  String body = "{\"client_id\":\"";
  body += CLIENT_ID;
  body += "\",\"round\":";
  body += String(current_round);
  body += ",\"n_samples\":";
  body += String(n_samples);
  body += ",\"weights\":[";
  for (int i = 0; i < TOTAL_WEIGHTS; i++) {
    body += String(flat[i], 6);
    if (i < TOTAL_WEIGHTS - 1) body += ",";
  }
  body += "]}";
  HTTPClient http;
  String url = String("http://") + SERVER_IP + ":" + SERVER_PORT + "/update";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();
  Serial.printf("[FL] Upload %s\n", code == 200 ? "OK ✓" : "FAILED ✗");
}

bool enableSensor(BLERemoteService* svc, BLEUUID cfgUUID, BLEUUID perUUID,
                  uint8_t periodVal = 100, uint8_t* cfgBytes = nullptr, size_t cfgLen = 1) {
  BLERemoteCharacteristic* cfg = svc->getCharacteristic(cfgUUID);
  BLERemoteCharacteristic* per = svc->getCharacteristic(perUUID);
  if (!cfg) return false;
  if (cfgBytes && cfgLen > 1) cfg->writeValue(cfgBytes, cfgLen, true);
  else { uint8_t on = 0x01; cfg->writeValue(&on, 1, true); }
  if (per) per->writeValue(&periodVal, 1, true);
  return true;
}

class MyCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String addr = advertisedDevice.getAddress().toString();
    if (addr.equalsIgnoreCase(SENSOR_TAG_MAC)) {
      Serial.println("SensorTag found!");
      BLEDevice::getScan()->stop();
      foundDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

void startScan() {
  foundDevice = nullptr;
  doConnect   = false;
  Serial.println("[BLE] Scanning...");
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyCallbacks());
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  scan->start(15, false);
}

#define MAX_SAMPLES  50
#define LOCAL_EPOCHS  5
float samples[MAX_SAMPLES][INPUT_DIM];

bool connectAndTrain() {
  Serial.println("[BLE] Connecting...");
  BLEClient* client = BLEDevice::createClient();
  client->setMTU(517);
  delay(500);
  if (!client->connect(foundDevice)) {
    Serial.println("[BLE] Connect failed");
    delete client; return false;
  }
  Serial.println("[BLE] Connected!");
  delay(500);

  BLERemoteService* movSvc = client->getService(movServiceUUID);
  BLERemoteService* tmpSvc = client->getService(tmpServiceUUID);
  BLERemoteService* humSvc = client->getService(humServiceUUID);
  BLERemoteService* optSvc = client->getService(optServiceUUID);

  if (!movSvc) {
    Serial.println("[BLE] Movement service missing!");
    client->disconnect(); delete client; return false;
  }

  uint8_t movCfg[2] = { 0x7F, 0x00 };
  enableSensor(movSvc, movConfigUUID, movPeriodUUID, 100, movCfg, 2);
  if (tmpSvc) enableSensor(tmpSvc, tmpConfigUUID, tmpPeriodUUID, 100);
  if (humSvc) enableSensor(humSvc, humConfigUUID, humPeriodUUID, 100);
  if (optSvc) enableSensor(optSvc, optConfigUUID, optPeriodUUID, 100);
  delay(3000);

  BLERemoteCharacteristic* movDat = movSvc->getCharacteristic(movDataUUID);
  BLERemoteCharacteristic* tmpDat = tmpSvc ? tmpSvc->getCharacteristic(tmpDataUUID) : nullptr;
  BLERemoteCharacteristic* humDat = humSvc ? humSvc->getCharacteristic(humDataUUID) : nullptr;
  BLERemoteCharacteristic* optDat = optSvc ? optSvc->getCharacteristic(optDataUUID) : nullptr;

  if (!movDat) { client->disconnect(); delete client; return false; }

  downloadModel();

  int collected = 0;
  Serial.println("[FL] Collecting samples...");

  while (collected < MAX_SAMPLES && client->isConnected()) {
    float s[INPUT_DIM] = {};

    String movVal = movDat->readValue();
    if (movVal.length() >= 18) {
      int16_t rawGX = (uint8_t)movVal[1]<<8|(uint8_t)movVal[0];
      int16_t rawGY = (uint8_t)movVal[3]<<8|(uint8_t)movVal[2];
      int16_t rawGZ = (uint8_t)movVal[5]<<8|(uint8_t)movVal[4];
      int16_t rawAX = (uint8_t)movVal[7]<<8|(uint8_t)movVal[6];
      int16_t rawAY = (uint8_t)movVal[9]<<8|(uint8_t)movVal[8];
      int16_t rawAZ = (uint8_t)movVal[11]<<8|(uint8_t)movVal[10];
      s[0] = rawAX / 16384.0f;
      s[1] = rawAY / 16384.0f;
      s[2] = rawAZ / 16384.0f;
      s[3] = rawGX / 131.072f / 250.0f;
      s[4] = rawGY / 131.072f / 250.0f;
      s[5] = rawGZ / 131.072f / 250.0f;
    }

    if (tmpDat) {
      String v = tmpDat->readValue();
      if (v.length() >= 4) {
        int16_t rawObj = (uint8_t)v[3]<<8|(uint8_t)v[2];
        s[6] = convertIRTemp(rawObj) / 50.0f;
      }
    }

    if (humDat) {
      String v = humDat->readValue();
      if (v.length() >= 4) {
        uint16_t rT = (uint8_t)v[1]<<8|(uint8_t)v[0];
        uint16_t rH = (uint8_t)v[3]<<8|(uint8_t)v[2];
        float t, h; convertHumidity(rT, rH, t, h);
        s[7] = h / 100.0f;
      }
    }

    if (optDat) {
      String v = optDat->readValue();
      if (v.length() >= 2) {
        uint16_t raw = (uint8_t)v[1]<<8|(uint8_t)v[0];
        s[8] = min(convertLux(raw) / 1000.0f, 1.0f);
      }
    }

    if (s[0] == 0 && s[1] == 0 && s[2] == 0) { delay(100); continue; }

    memcpy(samples[collected], s, sizeof(s));
    collected++;
    Serial.printf("  Sample %d — Accel: %.2f %.2f %.2f  Temp: %.1f°C\n",
      collected, s[0], s[1], s[2], s[6]*50.0f);
    delay(200);
  }

  Serial.printf("[FL] Collected %d samples\n", collected);

  if (collected < 2) {
    Serial.println("[FL] Not enough samples, skipping");
    client->disconnect(); delete client; return false;
  }

  for (int epoch = 0; epoch < LOCAL_EPOCHS; epoch++) {
    float loss = 0;
    for (int t = 0; t < collected - 1; t++) {
      float target[OUTPUT_DIM];
      for (int k = 0; k < OUTPUT_DIM; k++) target[k] = samples[t+1][k];
      sgd_step(samples[t], target);
      float h1[HIDDEN1], h2[HIDDEN2], out[OUTPUT_DIM];
      forward(samples[t], h1, h2, out);
      for (int k = 0; k < OUTPUT_DIM; k++) { float e = out[k]-target[k]; loss += e*e; }
    }
    Serial.printf("  Epoch %d  loss=%.4f\n", epoch+1, loss/(collected-1));
  }

  // ── DISCONNECT BLE FIRST then upload over WiFi ──────────────────────────
  Serial.println("[BLE] Disconnecting...");
  client->disconnect();
  delete client;
  delay(500);  // radio switches back to WiFi

  uploadWeights(collected);
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.print("[WiFi] Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.printf("\n[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
  BLEDevice::init("");
  startScan();
}

void loop() {
  if (doConnect && foundDevice) {
    connectAndTrain();
    delay(2000);
    startScan();
  }
  delay(100);
}