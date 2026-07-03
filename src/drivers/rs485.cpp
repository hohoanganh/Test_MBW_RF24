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

void rs485_send(const uint8_t *data, uint16_t len) {
  digitalWrite(RS485_DIR, HIGH);
  delayMicroseconds(20); // thoi gian chuyen mach transceiver
  SerialRS485.write(data, len);
  SerialRS485.flush();
  delayMicroseconds(20);
  digitalWrite(RS485_DIR, LOW);
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
