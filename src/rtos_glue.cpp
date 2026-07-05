#include "rtos_glue.h"

QueueHandle_t g_qToRF = NULL;
QueueHandle_t g_qToRS485 = NULL;
SemaphoreHandle_t g_muSPI = NULL;
SemaphoreHandle_t g_muSerial = NULL;
TaskHandle_t g_hTaskRS485 = NULL;
TaskHandle_t g_hTaskRF = NULL;
TaskHandle_t g_hTaskCLI = NULL;

// ===== CAP PHAT TINH (2026-07-03) =====
// Buffer nam trong .bss (RAM tinh, linker kiem tra luc build) thay vi malloc
// dong luc chay - tranh hoan toan rui ro "gioi han cap phat = con tro stack
// hien tai" cua allocator newlib (heap_useNewlib_ST.c) truoc khi
// vTaskStartScheduler() chay (xem ghi chu chi tiet trong platformio.ini).
static StaticQueue_t s_qToRF_ctrl;
static uint8_t s_qToRF_storage[RTOS_QUEUE_DEPTH * sizeof(rtos_frame_t)];
static StaticQueue_t s_qToRS485_ctrl;
static uint8_t s_qToRS485_storage[RTOS_QUEUE_DEPTH * sizeof(rtos_frame_t)];
static StaticSemaphore_t s_muSPI_ctrl;
static StaticSemaphore_t s_muSerial_ctrl;

void rtos_glue_init() {
  g_qToRF = xQueueCreateStatic(RTOS_QUEUE_DEPTH, sizeof(rtos_frame_t), s_qToRF_storage, &s_qToRF_ctrl);
  g_qToRS485 = xQueueCreateStatic(RTOS_QUEUE_DEPTH, sizeof(rtos_frame_t), s_qToRS485_storage, &s_qToRS485_ctrl);
  g_muSPI = xSemaphoreCreateMutexStatic(&s_muSPI_ctrl);
  g_muSerial = xSemaphoreCreateMutexStatic(&s_muSerial_ctrl);
}

void dbg_lock() {
  if (g_muSerial)
    xSemaphoreTake(g_muSerial, portMAX_DELAY);
}

void dbg_unlock() {
  if (g_muSerial)
    xSemaphoreGive(g_muSerial);
}

// ----- Watchdog: diem danh tung task (xem giai thich trong rtos_glue.h) -----
volatile uint32_t g_aliveRS485Ms = 0;
volatile uint32_t g_aliveRFMs = 0;
volatile uint32_t g_aliveCLIMs = 0;

bool rtos_all_tasks_alive() {
  uint32_t now = millis();
  return (uint32_t)(now - g_aliveRS485Ms) < RTOS_TASK_ALIVE_TIMEOUT_MS &&
         (uint32_t)(now - g_aliveRFMs) < RTOS_TASK_ALIVE_TIMEOUT_MS &&
         (uint32_t)(now - g_aliveCLIMs) < RTOS_TASK_ALIVE_TIMEOUT_MS;
}
