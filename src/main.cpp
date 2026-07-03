#include "mbw_common.h"

// ===== GLOBAL OBJECT =====
HardwareSerial SerialDBG(UART_DBG_RX, UART_DBG_TX);
HardwareSerial SerialRS485(RS485_RX, RS485_TX);

#include "hal.h"
#include "mbw_drv.h"
#include "rtos_glue.h"

// =========================================================
// KIEN TRUC RTOS (2026): chuyen tu setup()/loop() don khoi sang 3 TASK
// FreeRTOS toi gian de "song song hoa THAT SU" RF + RS485 + CLI (thay vi xu
// ly tuan tu trong 1 vong loop() nhu truoc). Chi tiet queue/mutex/RAM budget:
// xem src/rtos_glue.h va comment dau cac file rf_link.cpp/flashmem.cpp/bridge.cpp.
//
//   Task RS485/Bridge (uu tien CAO NHAT, RTOS_PRIO_RS485): doc RS485, phat
//     hien khoang lang 3.5 ky tu tach khung Modbus - KHONG duoc de task khac
//     lam tre qua han, neu khong se ghep sai khung.
//   Task RF          (uu tien trung binh, RTOS_PRIO_RF): nhan/gui RF that,
//     bao gom redundant TX co the mat vai trieu giay (delayMicroseconds) -
//     cach ly khoi RS485 de khong anh huong toi thoi gian tach khung.
//   Task CLI+ngoai vi (uu tien THAP NHAT, RTOS_PRIO_CLI): console CLI, DIP,
//     nut nhan, buzzer, LED heartbeat - it nhay cam thoi gian nhat.
//
// loop() (Idle task, sau vTaskStartScheduler()) PHAI KHONG BAO GIO BLOCK theo
// dung quy uoc cua STM32duino FreeRTOS - de trong, khong lam gi ca.
// =========================================================

static void task_rs485(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    rs485_process();  // "rs485mon" debug echo (chi active neu bat rieng)
    bridge_rs485_step();
    vTaskDelay(pdMS_TO_TICKS(1)); // nhuong CPU toi thieu, van kip thoi gian thuc voi gap 3.5 ky tu (>=1.75ms)
  }
}

static void task_rf(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    bridge_rf_step(); // rf_process() (nhan+heartbeat) + gui khung tu hang doi RS485
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

static void task_cli(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    cli_process();   // console CLI (id/ver/help/rs485/rf/bridge/rtos stat/...)
    dip_process();   // in "DIP: ..." khi DIP switch thay doi
    btn_process();   // in "BTN: DOWN/UP" khi nhan nut S2
    buzzer_update();  // tat buzzer dung gio (non-blocking)

    // ===== LED heartbeat =====
    static uint32_t t = 0;
    if (millis() - t > 500) {
      t = millis();
      led_life_toggle();
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // it nhay cam thoi gian nhat - nhuong CPU nhieu hon
  }
}

void setup() {

  hal_init();
  rtos_glue_init(); // tao queue/mutex TRUOC khi drv_init() (rf_init d.v. co the dung SPI ngay)
  drv_init();

  uart_log("SYSTEM INIT");
  uart_log("MBW RF24 RS485 2.0 FW " FW_VERSION " (" __DATE__ ")");
  uart_log("RTOS: 3 task (RS485/Bridge, RF, CLI+peripherals) - xem 'rtos stat'");
  uart_log("Type: help");

  // ===== DEBUG: kiem tra xTaskCreate co that bai khong (allocator thuc te la
  // newlib malloc - xem heap_useNewlib_ST.c - truoc vTaskStartScheduler() gioi
  // han cap phat la CON TRO STACK HIEN TAI, khong phai 1 con so co dinh, nen
  // KHONG duoc chen them lenh in xen giua 3 lan xTaskCreate (moi lan in ton
  // them stack ngay luc nhay cam nhat, de gay va cham heap/stack). Tao xong
  // ca 3 task ROI MOI in ket qua gon 1 lan. =====
  BaseType_t ok1 = xTaskCreate(task_rs485, "RS485", RTOS_STACK_RS485, NULL, RTOS_PRIO_RS485, &g_hTaskRS485);
  BaseType_t ok2 = xTaskCreate(task_rf, "RF", RTOS_STACK_RF, NULL, RTOS_PRIO_RF, &g_hTaskRF);
  BaseType_t ok3 = xTaskCreate(task_cli, "CLI", RTOS_STACK_CLI, NULL, RTOS_PRIO_CLI, &g_hTaskCLI);

  SerialDBG.print("TASK CREATE: RS485=");
  SerialDBG.print(ok1 == pdPASS ? "OK" : "FAIL");
  SerialDBG.print(" RF=");
  SerialDBG.print(ok2 == pdPASS ? "OK" : "FAIL");
  SerialDBG.print(" CLI=");
  SerialDBG.println(ok3 == pdPASS ? "OK" : "FAIL");
  // ===== HET PHAN DEBUG =====

  vTaskStartScheduler();

  // Chi toi day neu KHONG DU RAM de tao task Idle/Timer cua ban than FreeRTOS
  // (vTaskStartScheduler() binh thuong khong bao gio return). Bao loi qua UART
  // debug truc tiep (khong dung uart_log/SerialDBG object neu he thong RTOS da
  // hong mot phan) roi treo may - GIAM configTOTAL_HEAP_SIZE hoac stack tung
  // task (platformio.ini / rtos_glue.h) neu gap loi nay tren phan cung that.
  SerialDBG.println("FATAL: vTaskStartScheduler() failed (khong du RAM?)");
  while (1) {
  }
}

void loop() {
  // Sau vTaskStartScheduler(), ham nay tro thanh THAN CUA IDLE TASK (uu tien
  // thap nhat trong FreeRTOS) theo quy uoc cua STM32duino FreeRTOS - PHAI
  // KHONG BAO GIO BLOCK (khong delay()/delayMicroseconds() dai) vi Idle task
  // con lo don dep bo nho cac task da xoa. De trong la dung.
}
