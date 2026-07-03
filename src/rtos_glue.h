#pragma once
#include <Arduino.h>
#include <STM32FreeRTOS.h>

// =========================================================
// rtos_glue.* - "keo dan" RTOS dung chung giua 3 task (RS485/Bridge, RF,
// CLI+peripherals). Thay the kien truc loop() don khoi truoc day.
//
// 2 QUEUE thay cho goi ham truc tiep giua "chieu RS485" va "chieu RF":
//   g_qToRF    : RS485 task  -> RF task  (khung Modbus vua nhan tu RS485,
//                can gui khong day)
//   g_qToRS485 : RF task     -> RS485 task (khung vua nhan qua khong day,
//                can day ra RS485 that)
// Nho co queue, task RS485 KHONG BAO GIO bi khoa cung luc RF task dang gui
// lap redundant TX (co the mat vai trieu giay) - giu dung khoang lang 3.5 ky
// tu de tach khung Modbus chinh xac (day la ly do chinh chuyen sang RTOS).
//
// 2 MUTEX bao ve tai nguyen phan cung DUNG CHUNG giua nhieu task:
//   g_muSPI    : bus SPI1 dung chung boi nRF24L01 (rf_link.cpp) VA W25Q128
//                Flash (flashmem.cpp). Truoc day an toan vi loop() don luong;
//                nay CHAY THAT SU song song tren nhieu task nen BAT BUOC phai
//                co mutex, khong se co the 2 task tranh nhau tren cung 1 giao
//                dich SPI gay du lieu rac / crash.
//   g_muSerial : console Serial (SerialDBG) dung chung boi ca 3 task (CLI in
//                phan hoi lenh, RS485 task in "FWD RS485->RF", RF task in
//                "FWD RF->RS485"/"RF LINK ...") - khong khoa se bi xen ky tu
//                giua cac dong in cua 2 task khac nhau.
// =========================================================

#define RTOS_MODBUS_MAX_LEN 250 // dong bo voi MODBUS_MAX_LEN trong bridge.cpp

typedef struct {
  uint8_t data[RTOS_MODBUS_MAX_LEN];
  uint16_t len;
} rtos_frame_t;

// Do sau hang doi = 1: ung dung ban chat half-duplex (Modbus RTU hoi 1 - dap
// 1), khong can dem nhieu khung cung luc. Giu = 1 de tiet kiem RAM toi da tren
// chip chi 10KB RAM (moi slot ton RTOS_MODBUS_MAX_LEN+2 byte).
#define RTOS_QUEUE_DEPTH 1

// ----- Kich thuoc stack tung task (don vi WORD = 4 byte tren Cortex-M3,
// KHONG PHAI byte!). Da can chinh de vua RAM 10KB - neu board that bao
// "khong du RAM" luc tao task (xTaskCreate tra ve pdFAIL) hoac
// vTaskStartScheduler() khong bao gio chay toi dong lenh sau no, GIAM cac so
// nay truoc khi tang configTOTAL_HEAP_SIZE (platformio.ini). Uu tien giam
// RS485/RF truoc, CLI can nhieu stack hon vi dung sscanf/sprintf. -----
// 2026-07-03: giam RS485/RF tu 256 xuong 192 word - 2 task nay chi poll +
// gui queue, khong dung sprintf/sscanf nhu CLI, 256 word du thua. Muc dich:
// giai phong ~512 byte cho allocator (newlib malloc, xem heap_useNewlib_ST.c)
// truoc khi tao task CLI - lan dau CLI bi tao "OK" gia (heap/stack va cham,
// xPortGetFreeHeapSize() tra ve gia tri rac) vi allocator nay dung CON TRO
// STACK HIEN TAI lam gioi han truoc vTaskStartScheduler(), khong phai 1 vung
// heap co dinh - giam tong dung luong cap phat trong setup() la cach giam
// rui ro va cham thuc te (khong dua duoc configTOTAL_HEAP_SIZE, macro nay
// KHONG duoc allocator hien tai su dung - xem ghi chu trong platformio.ini).
// 2026-07-03: giam tiep 192->176 (RS485/RF) va 384->320 (CLI) de het tran
// RAM 808 byte khi link (region RAM overflowed). SAU KHI NAP PHAI kiem tra
// "rtos stat" (uxTaskGetStackHighWaterMark) tren board that - neu high water
// mark cua task nao < 32 word thi phai tang lai stack task do.
// 2026-07-03 (lan 2): sau khi chuyen rtos_frame_t (252B) trong bridge.cpp tu
// stack sang static (s_frame_rs485/s_frame_rf), stack RS485/RF khong con phai
// chua frame nua -> giam 176->160 word. CLI giu 320 (sscanf/sprintf).
#define RTOS_STACK_RS485 160 // 640 byte
#define RTOS_STACK_RF 160    // 640 byte
#define RTOS_STACK_CLI 320   // 1280 byte (sscanf/sprintf trong hal.cpp ton stack hon)

// Uu tien (so cang lon cang uu tien cao, tskIDLE_PRIORITY = 0). RS485 uu tien
// CAO NHAT de giu dung khoang lang 3.5 ky tu tach khung Modbus - bi task khac
// chiem CPU tre qua se lam hong viec phat hien bien khung.
#define RTOS_PRIO_RS485 (tskIDLE_PRIORITY + 3)
#define RTOS_PRIO_RF (tskIDLE_PRIORITY + 2)
#define RTOS_PRIO_CLI (tskIDLE_PRIORITY + 1)

extern QueueHandle_t g_qToRF;        // RS485 task -> RF task
extern QueueHandle_t g_qToRS485;     // RF task -> RS485 task
extern SemaphoreHandle_t g_muSPI;    // bao ve SPI1 dung chung (nRF24 + Flash)
extern SemaphoreHandle_t g_muSerial; // bao ve console Serial dung chung

// Handle cua 3 task - ghi tu main.cpp (xTaskCreate), doc tu hal.cpp cho lenh
// CLI "rtos stat" (in xPortGetFreeHeapSize() + uxTaskGetStackHighWaterMark()
// tung task de kiem tra RAM/stack con lai THUC TE tren phan cung).
extern TaskHandle_t g_hTaskRS485;
extern TaskHandle_t g_hTaskRF;
extern TaskHandle_t g_hTaskCLI;

void rtos_glue_init(); // tao queue/mutex - goi 1 lan trong setup() TRUOC khi tao task

// Helper in an toan giua nhieu task: lay g_muSerial, [in cac dong can in],
// tra lai. Dung bao NGOAI 1 khoi in nhieu dong lien tiep (vd 1 dong FWD/LINK
// hoan chinh, hoac toan bo phan hoi 1 lenh CLI) de tranh xen ky tu voi task
// khac dang in cung luc. KHONG goi long nhau (mutex khong dung kieu recursive).
void dbg_lock();
void dbg_unlock();
