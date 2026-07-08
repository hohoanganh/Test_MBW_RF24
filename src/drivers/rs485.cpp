#include "rs485.h"
#include "../mbw_common.h"
#include "../rtos_glue.h"
#include <string.h>

static uint32_t s_baud = RS485_BAUD_DEFAULT;
static bool s_monitor = false;

void rs485_init() {
  pinMode(RS485_DIR, OUTPUT);
  digitalWrite(RS485_DIR, LOW); // mac dinh: nghe (receive)
  SerialRS485.begin(s_baud);
}

uint32_t rs485_get_baud() { return s_baud; }

void rs485_set_baud(uint32_t b) {
  s_baud = b;
  SerialRS485.end();
  SerialRS485.begin(b);
  dbg_lock();
  SerialDBG.print("RS485 BAUD: ");
  SerialDBG.println(b);
  dbg_unlock();
}

// ----- 2026-07-07 (fix mat frame, lan 2): trang thai TX non-blocking -----
// Xem giai thich day du trong rs485.h. s_tx_pending/s_tx_start_us/s_tx_dur_us
// CHI duoc dung boi 1 nguoi goi tai 1 thoi diem (bridge.cpp, task RS485 - uu
// tien cao nhat, khong bi task khac xen giua) nen khong can mutex.
static bool s_tx_pending = false;
static uint32_t s_tx_start_us = 0;
static uint32_t s_tx_dur_us = 0;

void rs485_send_start(const uint8_t *data, uint16_t len) {
  digitalWrite(RS485_DIR, HIGH);
  delayMicroseconds(20); // thoi gian chuyen mach transceiver (ngan, khong dang ke so voi nguong 1.75ms)
  SerialRS485.write(data, len); // KHONG goi flush() - write() da tu copy du lieu vao buffer/driver, an toan de ham nay tra ve ngay
  uint32_t baud = s_baud ? s_baud : 9600;
  uint32_t char_us = (uint32_t)(10000000UL / baud); // us cho 1 ky tu (10 bit: start+8data+stop)
  s_tx_dur_us = (uint32_t)len * char_us + 300; // + bien an toan ~300us (jitter buffer/ngat)
  s_tx_start_us = micros();
  s_tx_pending = true;
}

bool rs485_send_pending() { return s_tx_pending; }

bool rs485_send_poll() {
  if (!s_tx_pending)
    return false;
  if ((uint32_t)(micros() - s_tx_start_us) < s_tx_dur_us)
    return false; // uoc luong: van dang truyen
  delayMicroseconds(20); // thoi gian chuyen mach transceiver truoc khi ve nghe
  digitalWrite(RS485_DIR, LOW);
  s_tx_pending = false;
  return true;
}

// BLOCKING - CHI danh cho CLI ("rs485 <text>")/rs485_loopback(), KHONG nam
// trong duong tach khung Modbus cua bridge (xem rs485.h). Dung lai chinh 3 ham
// non-blocking o tren (vong lap ngan cho toi khi xong) de khong trung logic.
void rs485_send(const uint8_t *data, uint16_t len) {
  rs485_send_start(data, len);
  while (!rs485_send_poll()) {
    // ban roi (uoc luong thoi gian TX con lai) - vong nay CHAP NHAN DUOC vi
    // cac noi goi ham nay (CLI/loopback) khong nam trong task RS485 uu tien
    // cao / khong anh huong toi tach khung Modbus.
  }
}

void rs485_send_str(const char *msg) {
  rs485_send((const uint8_t *)msg, (uint16_t)strlen(msg));
}

int rs485_available() { return SerialRS485.available(); }

int rs485_read() {
  if (!SerialRS485.available())
    return -1;
  return SerialRS485.read();
}

void rs485_set_monitor(bool en) { s_monitor = en; }
bool rs485_monitor_enabled() { return s_monitor; }

// "rs485mon": khi bat, forward moi byte RS485 nhan duoc ra console (debug).
// KHONG goi ham nay khi bridge dang bat (bridge.cpp da tu doc RS485 RX; goi
// ca 2 se lam mat byte vi SerialRS485.read() chi doc duoc 1 lan).
void rs485_process() {
  if (!s_monitor)
    return;
  // Khoa 1 lan cho CA CHUOI byte dang co (khong khoa/mo tung byte) de khong
  // bi xen dong "FWD .../RF LINK ..." cua task khac vao giua 1 chuoi dang in.
  if (!SerialRS485.available())
    return;
  dbg_lock();
  while (SerialRS485.available())
    SerialDBG.write(SerialRS485.read());
  dbg_unlock();
}

bool rs485_loopback() {
  while (SerialRS485.available())
    SerialRS485.read();

  const char *pat = "MBWTEST";
  rs485_send_str(pat);

  uint8_t i = 0;
  uint32_t t0 = millis();
  uint8_t patlen = (uint8_t)strlen(pat);

  while (millis() - t0 < 200) {
    if (SerialRS485.available()) {
      if ((char)SerialRS485.read() != pat[i])
        return false;
      if (++i == patlen)
        return true;
    }
  }
  return false;
}
