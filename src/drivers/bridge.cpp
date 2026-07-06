#include "bridge.h"
#include "rs485.h"
#include "rf_link.h"
#include "../mbw_common.h"
#include "../rtos_glue.h"
#include <string.h>

#define MODBUS_MAX_LEN 250 // gioi han theo RF_MAX_FRAG*RF_CHUNK_MAX (rf_link.h)
#define FWD_PREVIEW_MAX 16 // so byte in preview (hex) khi log forward

static bool s_enabled = true; // MAC DINH BAT: hanh vi chuan cua thiet bi
// 2026-07-06: log forward MAC DINH TAT cho lap dat that (Modbus master poll
// lien tuc 16 slave se in "FWD ..." day dac console + ton CPU in chuoi trong
// task uu tien cao). App test/kY thuat vien bat lai bang "bridge log on" khi
// can quan sat (app da co san nut toggle).
static bool s_log = false;
static uint8_t s_buf[MODBUS_MAX_LEN];
static uint16_t s_len = 0;
static uint32_t s_last_byte_us = 0;

// 2026-07-03: frame lam viec cua TUNG task chuyen tu stack sang STATIC.
// Truoc day moi rtos_frame_t (252 byte) nam tren stack cua
// bridge_rs485_step()/bridge_rf_step() lam TRAN STACK task RF (LED nhay 2
// xung - hook trong main.cpp) voi stack 176 word. Moi frame duoc dung TUAN TU
// trong 1 task duy nhat (s_frame_rs485 chi trong task RS485, s_frame_rf chi
// trong task RF) nen static la an toan, khong can mutex.
static rtos_frame_t s_frame_rs485; // CHI task RS485 dung (flush + nhan queue)
static rtos_frame_t s_frame_rf;    // CHI task RF dung (nhan queue + doc RF + echo debug)

static uint32_t s_cnt_rs485_to_rf = 0;
static uint32_t s_cnt_rf_to_rs485 = 0;
// Dem so khung bi ROT vi hang doi lien-task day (g_qToRF/g_qToRS485 sau = 1) -
// xem "rtos stat"/"bridge stat" (hal.cpp) de theo doi tren phan cung that.
static uint32_t s_cnt_rs485_to_rf_drop = 0;
static uint32_t s_cnt_rf_to_rs485_drop = 0;

void bridge_init() {
  s_len = 0;
  s_enabled = true; // forward RS485<->Wireless la hanh vi mac dinh cua san pham
  SerialDBG.println("BRIDGE: ON (mac dinh, forward RS485 <-> Wireless)");
}

void bridge_set_enable(bool en) {
  s_enabled = en;
  s_len = 0; // xoa buffer do dang khi doi mode, tranh ghep 2 khung khac nhau
  SerialDBG.println(en ? "BRIDGE: ON" : "BRIDGE: OFF");
}
bool bridge_is_enabled() { return s_enabled; }

void bridge_set_log(bool en) {
  s_log = en;
  SerialDBG.println(en ? "BRIDGE LOG: ON" : "BRIDGE LOG: OFF");
}
bool bridge_log_enabled() { return s_log; }

// In 1 dong ngan de app test hien thi truc quan hoat dong forward: huong,
// so byte, va vai byte dau dang HEX (khong in ASCII vi Modbus RTU la nhi phan).
// RTOS: co the duoc goi tu CA task RS485 (FWD RS485->RF) LAN task RF (FWD
// RF->RS485) - phai khoa g_muSerial (dbg_lock/unlock) de khong xen ky tu giua
// 2 dong in cua 2 task khac nhau.
static void log_forward(const char *dir, const uint8_t *buf, uint16_t len) {
  if (!s_log)
    return;
  dbg_lock();
  SerialDBG.print("FWD ");
  SerialDBG.print(dir);
  SerialDBG.print(": ");
  SerialDBG.print(len);
  SerialDBG.print(" bytes:");
  uint16_t n = len < FWD_PREVIEW_MAX ? len : FWD_PREVIEW_MAX;
  for (uint16_t i = 0; i < n; i++) {
    SerialDBG.print(' ');
    if (buf[i] < 0x10)
      SerialDBG.print('0');
    SerialDBG.print(buf[i], HEX);
  }
  if (len > n)
    SerialDBG.print(" ...");
  SerialDBG.println();
  dbg_unlock();
}

// Khoang lang gap khung ~3.5 ky tu (chuan Modbus RTU), tinh theo baud hien tai
// cua RS485 (10 bit/ky tu: start+8data+stop, khong parity).
static uint32_t frame_gap_us() {
  uint32_t baud = rs485_get_baud();
  if (baud == 0)
    baud = 9600;
  uint32_t char_us = (uint32_t)(10000000UL / baud); // us cho 1 ky tu
  uint32_t gap = (char_us * 35) / 10;                // 3.5 ky tu
  if (gap < 1750)
    gap = 1750; // toi thieu 1.75ms cho baud thap theo khuyen nghi Modbus
  return gap;
}

// Day 1 khung hoan chinh vua gom tu RS485 vao g_qToRF cho TASK RF gui that -
// KHONG goi rf_send() truc tiep tai day nua (rf_send co the mat vai trieu
// giay lap redundant TX, se lam task RS485 tre qua han phat hien khoang lang
// 3.5 ky tu cua khung Modbus KE TIEP). Non-blocking (timeout=0): hang doi day
// (dang xu ly khung truoc) -> ROT khung nay, dem vao s_cnt_rs485_to_rf_drop.
static void flush_rs485_to_rf() {
  if (s_len == 0)
    return;
  rtos_frame_t &f = s_frame_rs485; // static, xem ghi chu dau file
  f.len = s_len;
  memcpy(f.data, s_buf, s_len);
  if (xQueueSend(g_qToRF, &f, 0) == pdPASS) {
    s_cnt_rs485_to_rf++;
    log_forward("RS485->RF", s_buf, s_len);
  } else {
    s_cnt_rs485_to_rf_drop++;
  }
  s_len = 0;
}

// ----- Goi tu TASK RS485 (uu tien cao nhat) -----
void bridge_rs485_step() {
  // ----- Chieu RS485 -> RF: gom byte, phat hien khoang lang gap khung -----
  if (s_enabled) {
    int c;
    while ((c = rs485_read()) >= 0) {
      if (s_len < MODBUS_MAX_LEN)
        s_buf[s_len++] = (uint8_t)c;
      s_last_byte_us = micros();
    }
    if (s_len > 0 && (uint32_t)(micros() - s_last_byte_us) >= frame_gap_us()) {
      flush_rs485_to_rf();
    }
  }

  // ----- Chieu RF -> RS485: lay khung da nhan tu task RF (neu co), day ra
  // RS485 that. Non-blocking: khong co thi bo qua vong nay, thu lai vong sau. -----
  rtos_frame_t &f = s_frame_rs485; // static, xem ghi chu dau file
  if (xQueueReceive(g_qToRS485, &f, 0) == pdPASS) {
    if (s_enabled && f.len > 0) {
      rs485_send(f.data, f.len);
      s_cnt_rf_to_rs485++;
      log_forward("RF->RS485", f.data, f.len);
    }
    // Neu bridge dang OFF: khung van bi lay ra khoi hang doi (tranh ket dong
    // g_qToRS485 mai o trang thai day) nhung khong day ra RS485 that.
  }
}

// ----- Goi tu TASK RF (uu tien trung binh) -----
void bridge_rf_step() {
  rf_process(); // nhan khung RF + heartbeat - luon chay du bridge on/off

  if (s_enabled) {
    // Lay khung RS485->RF (neu co) tu task RS485, gui that qua RF (co the mat
    // vai trieu giay do redundant TX - khong sao vi task nay uu tien thap hon
    // RS485, khong lam tre viec tach khung Modbus).
    rtos_frame_t &f = s_frame_rf; // static, xem ghi chu dau file
    if (xQueueReceive(g_qToRF, &f, 0) == pdPASS) {
      rf_send(f.data, f.len);
    }

    // Chieu RF -> RS485: day khung nhan duoc vao g_qToRS485 cho task RS485.
    // Dung lai s_frame_rf (da xong viec voi khung gui o tren - tuan tu).
    if (rf_available()) {
      rtos_frame_t &out = s_frame_rf;
      uint16_t n = rf_read(out.data, sizeof(out.data));
      if (n > 0) {
        out.len = n;
        if (xQueueSend(g_qToRS485, &out, 0) != pdPASS)
          s_cnt_rf_to_rs485_drop++;
      }
    }
  } else if (rf_available()) {
    // Bridge OFF: giu lai hanh vi debug cu (khong forward, chi echo ra console
    // de ky thuat vien xem thu RF co nhan duoc gi khong khi dang test rieng
    // le cac lenh CLI "rf tx <text>"/"rs485 <text>").
    // Dung lai s_frame_rf.data thay vi them 250B tren stack (xem ghi chu dau file)
    uint8_t *buf = s_frame_rf.data;
    uint16_t n = rf_read(buf, sizeof(s_frame_rf.data) - 1);
    buf[n] = 0;
    dbg_lock();
    SerialDBG.print("RF RX: ");
    SerialDBG.println((const char *)buf);
    dbg_unlock();
  }
}

void bridge_get_stats(uint32_t *rs485_to_rf, uint32_t *rf_to_rs485) {
  if (rs485_to_rf) *rs485_to_rf = s_cnt_rs485_to_rf;
  if (rf_to_rs485) *rf_to_rs485 = s_cnt_rf_to_rs485;
}
void bridge_get_drop_stats(uint32_t *rs485_to_rf_drop, uint32_t *rf_to_rs485_drop) {
  if (rs485_to_rf_drop) *rs485_to_rf_drop = s_cnt_rs485_to_rf_drop;
  if (rf_to_rs485_drop) *rf_to_rs485_drop = s_cnt_rf_to_rs485_drop;
}
void bridge_reset_stats() {
  s_cnt_rs485_to_rf = s_cnt_rf_to_rs485 = 0;
  s_cnt_rs485_to_rf_drop = s_cnt_rf_to_rs485_drop = 0;
}
