#include "rtos_glue.h"

QueueHandle_t g_qToRF = NULL;
QueueHandle_t g_qToRS485 = NULL;
SemaphoreHandle_t g_muSPI = NULL;
SemaphoreHandle_t g_muSerial = NULL;
TaskHandle_t g_hTaskRS485 = NULL;
TaskHandle_t g_hTaskRF = NULL;
TaskHandle_t g_hTaskCLI = NULL;

void rtos_glue_init() {
  g_qToRF = xQueueCreate(RTOS_QUEUE_DEPTH, sizeof(rtos_frame_t));
  g_qToRS485 = xQueueCreate(RTOS_QUEUE_DEPTH, sizeof(rtos_frame_t));
  g_muSPI = xSemaphoreCreateMutex();
  g_muSerial = xSemaphoreCreateMutex();
}

void dbg_lock() {
  if (g_muSerial)
    xSemaphoreTake(g_muSerial, portMAX_DELAY);
}

void dbg_unlock() {
  if (g_muSerial)
    xSemaphoreGive(g_muSerial);
}
