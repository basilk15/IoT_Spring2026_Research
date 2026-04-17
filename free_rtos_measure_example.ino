#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if (configUSE_TRACE_FACILITY != 1) || (configGENERATE_RUN_TIME_STATS != 1)
#error "This example requires FreeRTOS trace facility and run-time stats enabled in the ESP32 Arduino core."
#endif

static constexpr const char *kWorkerTaskName = "ModExpTask";
static constexpr UBaseType_t kWorkerPriority = 2;
static constexpr uint32_t kMeasurementIntervalMs = 8000;
static constexpr uint32_t kWarmupDelayMs = 2000;
static constexpr size_t kMaxTrackedTasks = 24;

TaskHandle_t gWorkerHandle = nullptr;

struct TaskRuntimeSnapshot {
  UBaseType_t taskNumber;
  char name[configMAX_TASK_NAME_LEN];
  configRUN_TIME_COUNTER_TYPE runTimeCounter;
};

struct StatsSnapshot {
  TaskRuntimeSnapshot tasks[kMaxTrackedTasks];
  UBaseType_t count;
  configRUN_TIME_COUNTER_TYPE totalRunTime;
};

uint32_t modExp(uint32_t base, uint32_t exponent, uint32_t modulus) {
  uint64_t result = 1;
  uint64_t value = base % modulus;

  while (exponent > 0) {
    if (exponent & 1U) {
      result = (result * value) % modulus;
    }
    value = (value * value) % modulus;
    exponent >>= 1U;
  }

  return static_cast<uint32_t>(result);
}

void captureStats(StatsSnapshot &snapshot) {
  snapshot.count = 0;
  snapshot.totalRunTime = 0;

  UBaseType_t taskCount = uxTaskGetNumberOfTasks();
  if (taskCount > kMaxTrackedTasks) {
    taskCount = kMaxTrackedTasks;
  }

  TaskStatus_t *statusArray = static_cast<TaskStatus_t *>(pvPortMalloc(taskCount * sizeof(TaskStatus_t)));
  if (statusArray == nullptr) {
    Serial.println("Failed to allocate task stats buffer.");
    return;
  }

  UBaseType_t actualCount = uxTaskGetSystemState(statusArray, taskCount, &snapshot.totalRunTime);
  if (actualCount > kMaxTrackedTasks) {
    actualCount = kMaxTrackedTasks;
  }

  snapshot.count = actualCount;
  for (UBaseType_t i = 0; i < actualCount; ++i) {
    snapshot.tasks[i].taskNumber = statusArray[i].xTaskNumber;
    strncpy(snapshot.tasks[i].name, statusArray[i].pcTaskName, sizeof(snapshot.tasks[i].name) - 1);
    snapshot.tasks[i].name[sizeof(snapshot.tasks[i].name) - 1] = '\0';
    snapshot.tasks[i].runTimeCounter = statusArray[i].ulRunTimeCounter;
  }

  vPortFree(statusArray);
}

configRUN_TIME_COUNTER_TYPE findRuntimeByTaskNumber(const StatsSnapshot &snapshot, UBaseType_t taskNumber) {
  for (UBaseType_t i = 0; i < snapshot.count; ++i) {
    if (snapshot.tasks[i].taskNumber == taskNumber) {
      return snapshot.tasks[i].runTimeCounter;
    }
  }
  return 0;
}

void printDeltaReport(const StatsSnapshot &before, const StatsSnapshot &after) {
  if (after.totalRunTime <= before.totalRunTime) {
    Serial.println("Run-time counter did not advance.");
    return;
  }

  configRUN_TIME_COUNTER_TYPE totalDelta = after.totalRunTime - before.totalRunTime;
  Serial.println("Task CPU usage during modular exponentiation window:");
  Serial.println("Task Name         Delta(us)   Share");

  for (UBaseType_t i = 0; i < after.count; ++i) {
    const auto &task = after.tasks[i];
    configRUN_TIME_COUNTER_TYPE beforeCounter = findRuntimeByTaskNumber(before, task.taskNumber);
    if (task.runTimeCounter < beforeCounter) {
      continue;
    }

    configRUN_TIME_COUNTER_TYPE delta = task.runTimeCounter - beforeCounter;
    if (delta == 0) {
      continue;
    }

    float share = (100.0f * static_cast<float>(delta)) / static_cast<float>(totalDelta);
    Serial.printf("%-16s %-10lu %.2f%%\n", task.name, static_cast<unsigned long>(delta), share);
  }

  Serial.printf("TOTAL_RUNTIME_DELTA(us): %lu\n", static_cast<unsigned long>(totalDelta));
  Serial.println();
}

void modExpTask(void *parameter) {
  uint32_t jobId = 0;

  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    ++jobId;
    uint32_t accumulator = 0;

    for (uint32_t i = 0; i < 6000; ++i) {
      uint32_t base = 1234567U + i;
      uint32_t exponent = 65537U + (i % 97U);
      uint32_t modulus = 2147483647U;
      accumulator ^= modExp(base, exponent, modulus);
    }

    Serial.printf("ModExp job %lu finished, checksum=%lu\n",
                  static_cast<unsigned long>(jobId),
                  static_cast<unsigned long>(accumulator));

    xTaskNotifyGive(static_cast<TaskHandle_t>(parameter));
  }
}

void setup() {
  Serial.begin(115200);
  delay(kWarmupDelayMs);

  Serial.println();
  Serial.println("FreeRTOS modular exponentiation CPU measurement example");
  Serial.println("The worker task is measured using FreeRTOS run-time stats.");
  Serial.println();

  BaseType_t ok = xTaskCreatePinnedToCore(
    modExpTask,
    kWorkerTaskName,
    4096,
    xTaskGetCurrentTaskHandle(),
    kWorkerPriority,
    &gWorkerHandle,
    1
  );

  if (ok != pdPASS) {
    Serial.println("Failed to create ModExpTask.");
    while (true) {
      delay(1000);
    }
  }
}

void loop() {
  static uint32_t roundId = 0;
  StatsSnapshot before = {};
  StatsSnapshot after = {};

  ++roundId;
  Serial.printf("Measurement round %lu\n", static_cast<unsigned long>(roundId));

  captureStats(before);
  xTaskNotifyGive(gWorkerHandle);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  captureStats(after);

  printDeltaReport(before, after);
  delay(kMeasurementIntervalMs);
}
