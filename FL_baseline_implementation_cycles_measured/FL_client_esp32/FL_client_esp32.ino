#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <math.h>

const char* WIFI_SSID         = "pop-os";
const char* WIFI_PASS         = "111222333";
const char* SERVER_IP         = "10.120.60.169";
const int   SERVER_PORT       = 5000;
const char* CLIENT_ID         = "esp32-node-A";

const int   ROUND_INTERVAL_MS = 30000;
const int   LOCAL_SAMPLES     = 50;
const int   LOCAL_EPOCHS      = 5;
const float LEARNING_RATE     = 0.01f;

#define INPUT_DIM     9
#define HIDDEN1      16
#define HIDDEN2       8
#define OUTPUT_DIM    4
#define TOTAL_WEIGHTS 332  // (9*16+16)+(16*8+8)+(8*4+4) = 160+136+36 = 332

float W1[INPUT_DIM][HIDDEN1], b1[HIDDEN1];
float W2[HIDDEN1][HIDDEN2],   b2[HIDDEN2];
float W3[HIDDEN2][OUTPUT_DIM],b3[OUTPUT_DIM];
int   current_round = 0;

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

void sgd_step(const float* x, const float* target) {
  float h1[HIDDEN1], h2[HIDDEN2], out[OUTPUT_DIM];
  forward(x, h1, h2, out);

  float dout[OUTPUT_DIM];
  for (int j = 0; j < OUTPUT_DIM; j++)
    dout[j] = 2.0f * (out[j] - target[j]) / OUTPUT_DIM;

  float dh2[HIDDEN2] = {};
  for (int i = 0; i < HIDDEN2; i++) {
    for (int j = 0; j < OUTPUT_DIM; j++) {
      W3[i][j] -= LEARNING_RATE * dout[j] * h2[i];
      dh2[i]   += dout[j] * W3[i][j];
    }
  }
  for (int j = 0; j < OUTPUT_DIM; j++) b3[j] -= LEARNING_RATE * dout[j];
  for (int i = 0; i < HIDDEN2; i++) dh2[i] = (h2[i] > 0) ? dh2[i] : 0;

  float dh1[HIDDEN1] = {};
  for (int i = 0; i < HIDDEN1; i++) {
    for (int j = 0; j < HIDDEN2; j++) {
      W2[i][j] -= LEARNING_RATE * dh2[j] * h1[i];
      dh1[i]   += dh2[j] * W2[i][j];
    }
  }
  for (int j = 0; j < HIDDEN2; j++) b2[j] -= LEARNING_RATE * dh2[j];
  for (int i = 0; i < HIDDEN1; i++) dh1[i] = (h1[i] > 0) ? dh1[i] : 0;

  for (int i = 0; i < INPUT_DIM; i++)
    for (int j = 0; j < HIDDEN1; j++)
      W1[i][j] -= LEARNING_RATE * dh1[j] * x[i];
  for (int j = 0; j < HIDDEN1; j++) b1[j] -= LEARNING_RATE * dh1[j];
}

// MPU6050 — 6 real features + 3 zeros to match INPUT_DIM=9
bool read_sensors(float* out9) {
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(0x68, 14);
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  Wire.read(); Wire.read();  // skip temp
  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();
  out9[0] = ax / 16384.0f;
  out9[1] = ay / 16384.0f;
  out9[2] = az / 16384.0f;
  out9[3] = gx / 131.0f / 250.0f;
  out9[4] = gy / 131.0f / 250.0f;
  out9[5] = gz / 131.0f / 250.0f;
  out9[6] = 0.0f;  // no temp on node-A
  out9[7] = 0.0f;  // no humidity on node-A
  out9[8] = 0.0f;  // no lux on node-A
  return true;
}

void download_model() {
  HTTPClient http;
  String url = String("http://") + SERVER_IP + ":" + SERVER_PORT + "/model";
  http.begin(url);
  if (http.GET() != 200) { http.end(); Serial.println("[FL] GET failed"); return; }
  String payload = http.getString();
  http.end();

  float flat[TOTAL_WEIGHTS];
  if (parseModelResponse(payload, current_round, flat)) {
    unflattenWeights(flat);
    Serial.printf("[FL] Model fetched (round %d)\n", current_round);
  } else {
    Serial.println("[FL] Parse failed");
  }
}

void upload_weights(int n_samples) {
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

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.beginTransmission(0x68);
  Wire.write(0x6B); Wire.write(0x00);
  Wire.endTransmission();

  Serial.print("[WiFi] Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.printf("\n[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
}

void loop() {
  Serial.println("\n── Starting federated round ──");

  uint32_t round_start_cycles = ESP.getCycleCount();

  uint32_t download_start_cycles = ESP.getCycleCount();
  download_model();
  uint32_t download_used_cycles = ESP.getCycleCount() - download_start_cycles;
  Serial.printf("[CYCLES] download_model=%lu\n", (unsigned long)download_used_cycles);

  float samples[LOCAL_SAMPLES][INPUT_DIM];
  int collected = 0;
  uint32_t sensor_collection_start_cycles = ESP.getCycleCount();
  while (collected < LOCAL_SAMPLES) {
    float s[INPUT_DIM];
    if (read_sensors(s)) {
      memcpy(samples[collected], s, sizeof(s));
      collected++;
    }
    delay(20);
  }
  uint32_t sensor_collection_used_cycles = ESP.getCycleCount() - sensor_collection_start_cycles;
  Serial.printf("[CYCLES] sensor_collection=%lu\n", (unsigned long)sensor_collection_used_cycles);
  Serial.printf("[FL] Collected %d samples\n", collected);

  uint32_t local_training_start_cycles = ESP.getCycleCount();
  for (int epoch = 0; epoch < LOCAL_EPOCHS; epoch++) {
    float loss = 0;
    for (int t = 0; t < collected - 1; t++) {
      float target[OUTPUT_DIM];
      for (int k = 0; k < OUTPUT_DIM; k++) target[k] = samples[t+1][k];
      sgd_step(samples[t], target);
      float h1[HIDDEN1], h2[HIDDEN2], out[OUTPUT_DIM];
      forward(samples[t], h1, h2, out);
      for (int k = 0; k < OUTPUT_DIM; k++) {
        float e = out[k] - target[k];
        loss += e * e;
      }
    }
    Serial.printf("  Epoch %d  loss=%.4f\n", epoch + 1, loss / (collected - 1));
  }
  uint32_t local_training_used_cycles = ESP.getCycleCount() - local_training_start_cycles;
  Serial.printf("[CYCLES] local_training_total=%lu\n", (unsigned long)local_training_used_cycles);

  uint32_t upload_start_cycles = ESP.getCycleCount();
  upload_weights(collected);
  uint32_t upload_used_cycles = ESP.getCycleCount() - upload_start_cycles;
  Serial.printf("[CYCLES] upload_weights=%lu\n", (unsigned long)upload_used_cycles);

  uint32_t round_used_cycles = ESP.getCycleCount() - round_start_cycles;
  Serial.printf("[CYCLES] round_total=%lu\n", (unsigned long)round_used_cycles);
  delay(ROUND_INTERVAL_MS);
}
