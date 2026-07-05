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
// loop() (Idle task, sau vTaskStartScheduler()) KHONG con "de trong" nua tu
// 2026-07-05: day la noi DUY NHAT feed watchdog IWDG - xem giai thich day du
// trong drivers/watchdog.h va rtos_glue.h (muc "Watchdog: diem danh tung
// task"). Van PHAI KHONG BAO GIO BLOCK theo dung quy uoc cua STM32duino
// FreeRTOS (khong delay()/delayMicroseconds() dai) - wdt_feed() chi la 1 lenh
// ghi thanh ghi, khong block.
// =========================================================

static void task_rs485(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    g_aliveRS485Ms = millis(); // "diem danh" cho watchdog - xem rtos_glue.h
    rs485_process();  // "rs485mon" debug echo (chi active neu bat rieng)
    bridge_rs485_step();
    vTaskDelay(pdMS_TO_TICKS(1)); // nhuong CPU toi thieu, van kip thoi gian thuc voi gap 3.5 ky tu (>=1.75ms)
  }
}

static void task_rf(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    g_aliveRFMs = millis(); // "diem danh" cho watchdog - xem rtos_glue.h
    bridge_rf_step(); // rf_process() (nhan+heartbeat) + gui khung tu hang doi RS485
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// ===== CAP PHAT TINH cho 3 task (2026-07-03) - xem ghi chu chi tiet trong
// platformio.ini / rtos_glue.cpp. Stack + TCB nam trong .bss, RAM co dinh
// duoc linker kiem tra luc build, khong con phu thuoc con tro stack luc
// runtime nua. =====
static StaticTask_t s_tcbRS485, s_tcbRF, s_tcbCLI;
static StackType_t s_stackRS485[RTOS_STACK_RS485];
static StackType_t s_stackRF[RTOS_STACK_RF];
static StackType_t s_stackCLI[RTOS_STACK_CLI];

// Callback BAT BUOC phai co khi configSUPPORT_STATIC_ALLOCATION=1 (FreeRTOS
// kernel tu goi de lay buffer cho Idle task noi bo, du ta khong tao Idle task
// truc tiep). KHONG can vApplicationGetTimerTaskMemory vi configUSE_TIMERS=0
// (khong tao Timer service task).
extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                               StackType_t **ppxIdleTaskStackBuffer,
                                               uint32_t *pulIdleTaskStackSize) {
  // configMINIMAL_STACK_SIZE cua STM32duino tinh tu linker symbol
  // (_Min_Stack_Size) nen KHONG phai hang so compile-time -> dung size co dinh.
  enum { IDLE_STACK_WORDS = 128 }; // 2026-07-03: tang lai 96->128 word khi
                                   // debug treo (idle hook cua STM32duino GOI
                                   // loop() moi vong - khong hoan toan "rong")
  static StaticTask_t s_tcbIdle;
  static StackType_t s_stackIdle[IDLE_STACK_WORDS];
  *ppxIdleTaskTCBBuffer = &s_tcbIdle;
  *ppxIdleTaskStackBuffer = s_stackIdle;
  *pulIdleTaskStackSize = IDLE_STACK_WORDS;
}

// ===== Hook bao TRAN STACK (configCHECK_FOR_STACK_OVERFLOW=2, xem
// STM32FreeRTOSConfig.h). Duoc kernel goi ngay khi phat hien pattern cuoi
// stack bi ghi de luc chuyen context. KHONG in duoc qua SerialDBG o day
// (hook chay trong ngu canh ngat, TX interrupt-driven se deadlock) -> bao
// bang LED_LIFE: nhay N xung ngan roi nghi dai, lap vo han:
//   1 xung = RS485, 2 = RF, 3 = CLI, 4 = IDLE, 5 = khac
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
  (void)xTask;
  taskDISABLE_INTERRUPTS();
  int n = 5;
  if (pcTaskName) {
    if (pcTaskName[0]=='R' && pcTaskName[1]=='S') n = 1;      // "RS485"
    else if (pcTaskName[0]=='R' && pcTaskName[1]=='F') n = 2; // "RF"
    else if (pcTaskName[0]=='C') n = 3;                       // "CLI"
    else if (pcTaskName[0]=='I') n = 4;                       // "IDLE"
  }
  pinMode(LED_LIFE, OUTPUT);
  for (;;) {
    for (int i = 0; i < n; i++) {
      digitalWrite(LED_LIFE, HIGH);
      for (volatile uint32_t d = 0; d < 400000; d++) {}  // ~xung ngan
      digitalWrite(LED_LIFE, LOW);
      for (volatile uint32_t d = 0; d < 400000; d++) {}
    }
    for (volatile uint32_t d = 0; d < 3200000; d++) {}   // nghi dai giua chu ky
  }
}

static void task_cli(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    g_aliveCLIMs = millis(); // "diem danh" cho watchdog - xem rtos_glue.h
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

  // 2026-07-05: bat IWDG CANG SOM CANG TOT - truoc drv_init()/tao task, vi
  // mot khi da begin() thi KHONG THE tat lai (dac tinh phan cung, xem
  // drivers/watchdog.h). wdt_init() tu doc+cache co "reset boi watchdog" cua
  // lan khoi dong nay TRUOC khi begin() (xem watchdog.cpp). Timeout 8 giay:
  // du du so voi thao tac cham nhat trong firmware (flash erase sector ~vai
  // tram ms, redundant TX toi da 6 lan x nhieu manh) de khong reset nham,
  // nhung van phuc hoi duoc trong thoi gian hop ly neu MCU treo that su.
  wdt_init(8000000UL); // 8,000,000 us = 8 giay

  drv_init();

  uart_log("SYSTEM INIT");
  uart_log("MBW RF24 RS485 2.0 FW " FW_VERSION " (" __DATE__ ")");
  uart_log("RTOS: 3 task (RS485/Bridge, RF, CLI+peripherals) - xem 'rtos stat'");
  uart_log("WATCHDOG: IWDG 8s, feed theo suc khoe tung task - xem 'wdt stat'");
  if (wdt_boot_was_reset())
    uart_log("CANH BAO: LAN KHOI DONG TRUOC BI WATCHDOG RESET (MCU da treo)!");
  uart_log("Type: help");

  // xTaskCreateStatic tra ve NULL neu that bai (khong con phu thuoc heap dong
  // luc setup() nua - xem ghi chu dau file).
  g_hTaskRS485 = xTaskCreateStatic(task_rs485, "RS485", RTOS_STACK_RS485, NULL, RTOS_PRIO_RS485, s_stackRS485, &s_tcbRS485);
  g_hTaskRF = xTaskCreateStatic(task_rf, "RF", RTOS_STACK_RF, NULL, RTOS_PRIO_RF, s_stackRF, &s_tcbRF);
  g_hTaskCLI = xTaskCreateStatic(task_cli, "CLI", RTOS_STACK_CLI, NULL, RTOS_PRIO_CLI, s_stackCLI, &s_tcbCLI);

  SerialDBG.print("TASK CREATE: RS485=");
  SerialDBG.print(g_hTaskRS485 ? "OK" : "FAIL");
  SerialDBG.print(" RF=");
  SerialDBG.print(g_hTaskRF ? "OK" : "FAIL");
  SerialDBG.print(" CLI=");
  SerialDBG.println(g_hTaskCLI ? "OK" : "FAIL");

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
  // con lo don dep bo nho cac task da xoa.
  //
  // 2026-07-05: VIEC DUY NHAT lam o day la feed watchdog IWDG, va CHI feed
  // NEU CA 3 task RTOS deu vua "diem danh" gan day (rtos_all_tasks_alive(),
  // xem rtos_glue.h). Neu 1 task bi treo that su, ham nay tra ve false sau
  // toi da RTOS_TASK_ALIVE_TIMEOUT_MS, IWDG khong con duoc feed -> MCU tu
  // RESET sau khi het thoi gian timeout (wdt_init() trong setup()). Day la 1
  // lenh ghi thanh ghi don gian (wdt_feed()), khong block, an toan trong Idle
  // task.
  if (rtos_all_tasks_alive())
    wdt_feed();
}
