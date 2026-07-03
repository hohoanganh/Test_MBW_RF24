// =====================================================================
//  OPMS 1.6 Slave Board - Firmware test (FreeRTOS)
//  ---------------------------------------------------------------------
//  Build / nap:  pio run            |  pio run -t upload
//  Monitor    :  pio device monitor (Console USART1, 115200)
//
//  3 task FreeRTOS:
//    - Console  : doc UART console, parse CLI, thuc thi lenh (cli_process).
//    - Heartbeat: nhay LED LIFE 1Hz + cap nhat buzzer (chay ngay ca khi dang do).
//    - Comm     : monitor Orange Pi (USART3) + RS485 o NEN, in RX ra console.
//  Mutex bao ve UART console (Console & Comm cung in).
//
//  Cac driver dung delay()/pulseIn() (busy-wait): duoi FreeRTOS, task khac
//  van duoc preempt nen LED LIFE khong bi dung khi mot lenh dang chay.
// =====================================================================
#include "opms_common.h"

// Console USART1 (PA9 = TX, PA10 = RX)
HardwareSerial SerialDBG(UART_DBG_RX, UART_DBG_TX);

#include "hal.h"
#include "opms_drv.h"
#include <STM32FreeRTOS.h>

// Mutex bao ve UART console (nhieu task cung in -> tranh xen ke)
static SemaphoreHandle_t gUartMtx;

// Bat/tat monitor nen Orange Pi. (RS485 monitor/txloop dieu khien qua rs485_bg_*)
// MAC DINH TAT: tranh spam "PI_RX>" lam nghet console khi chan RX bi nhieu/tha noi.
static volatile bool gMonitorPI = false;

// Accessor cho CLI (hal.cpp) bat/tat monitor Orange Pi.
void pi_monitor_set(bool on) { gMonitorPI = on; }
bool pi_monitor_get()        { return gMonitorPI; }

// ---------------------------------------------------------------------
//  Task Console: doc + thuc thi lenh CLI
// ---------------------------------------------------------------------
static void vConsoleTask(void *pv) {
  (void)pv;
  cli_prompt();
  for (;;) {
    if (xSemaphoreTake(gUartMtx, portMAX_DELAY) == pdTRUE) {
      cli_process();                 // non-blocking: chi chay lenh khi du 1 dong
      xSemaphoreGive(gUartMtx);
    }
    vTaskDelay(pdMS_TO_TICKS(2));     // nhuong CPU
  }
}

// ---------------------------------------------------------------------
//  Task Heartbeat: LED LIFE 1Hz + buzzer
// ---------------------------------------------------------------------
static void vHeartbeatTask(void *pv) {
  (void)pv;
  uint16_t c = 0;
  for (;;) {
    buzzer_update();
    if (++c >= 50) {                 // 50 x 10ms = 500ms -> doi muc 1Hz
      c = 0;
      led_life_toggle();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---------------------------------------------------------------------
//  Task Comm: monitor Orange Pi (USART3) + RS485 o nen
// ---------------------------------------------------------------------
static void vCommTask(void *pv) {
  (void)pv;
  uint8_t buf[64];
  for (;;) {
    if (gMonitorPI) {
      int n = pi_read_avail(buf, sizeof(buf));
      if (n > 0 && xSemaphoreTake(gUartMtx, pdMS_TO_TICKS(50)) == pdTRUE) {
        SerialDBG.print("PI_RX> ");
        for (int i = 0; i < n; i++) SerialDBG.write(buf[i]);
        SerialDBG.println();
        xSemaphoreGive(gUartMtx);
      }
    }
    // RS485 monitor/txloop chay o NEN (lenh rs485mon / rs485loop) - khong block CLI
    if (rs485_bg_mode() && xSemaphoreTake(gUartMtx, pdMS_TO_TICKS(50)) == pdTRUE) {
      rs485_bg_poll();
      xSemaphoreGive(gUartMtx);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------------------------------------------------------------------
void setup() {
  hal_init();
  drv_init();

  uart_log("");
  uart_log("==================================================");
  uart_log("  >>> OPMS 1.6 SLAVE - FreeRTOS <<<");
  uart_log("  MCU   : STM32F303VCT6");
  uart_log("  FW    : " FW_VERSION " (RTOS)");
  uart_log("  Build : " __DATE__ " " __TIME__);
  uart_log("  Tasks : Console / Heartbeat / Comm");
  uart_log("==================================================");
  uart_log("Go 'help' de xem danh sach lenh.");

  gUartMtx = xSemaphoreCreateMutex();

  // Stack tinh theo WORD. Console can nhieu (snprintf + driver).
  xTaskCreate(vConsoleTask,   "cli",  512, NULL, 2, NULL);
  xTaskCreate(vHeartbeatTask, "hb",   128, NULL, 1, NULL);
  xTaskCreate(vCommTask,      "comm", 256, NULL, 1, NULL);

  vTaskStartScheduler();           // khong tro ve (scheduler tiep quan)
}

void loop() {
  // Khong dung - FreeRTOS scheduler da chay.
}
