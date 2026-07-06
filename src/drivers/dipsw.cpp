#include "dipsw.h"
#include "../mbw_common.h"
#include "../rtos_glue.h"

// 74HC165: khong dung CLK_INH tren board nay (luon cho phep dich, xem
// docs/Hardware.md) -> chi can 1 xung LOAD + 8 xung CLK.
void dip_init() {
  pinMode(SW_LOAD, OUTPUT);
  digitalWrite(SW_LOAD, HIGH);
  pinMode(SW_SCK, OUTPUT);
  digitalWrite(SW_SCK, LOW);
  pinMode(SW_MISO, INPUT);
}

uint8_t dip_read_raw() {
  // Chot du lieu song song (SH/LD muc thap = load)
  digitalWrite(SW_LOAD, LOW);
  delayMicroseconds(5);
  digitalWrite(SW_LOAD, HIGH);

  uint8_t val = 0;
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t bit = digitalRead(SW_MISO) ? 1 : 0; // QH: bit dau tien la MSB (H)
    val |= (uint8_t)(bit << (7 - i));
    digitalWrite(SW_SCK, HIGH);
    delayMicroseconds(2);
    digitalWrite(SW_SCK, LOW);
    delayMicroseconds(2);
  }
  // 2026-07-06 - DA DOI CHIEU TREN BOARD THAT: chan 74HC165 co pull-up, gat
  // switch ON = noi GND -> muc dien 0; OFF = 1. Doc tho all-OFF ra 0xFF
  // (DEVID=63, BAUD=19200) la SAI quy uoc "DIP de nguyen = 0 = HUB". DAO BIT
  // tai day de moi noi khac trong firmware dung logic thuan: bit=1 nghia la
  // switch dang ON (gat ve phia GND); all-OFF -> 0x00 -> DEVID=0 (HUB),
  // BAUD=4800 (bang chon SW7-8: 00=4800).
  return (uint8_t)~val;
}

uint8_t dip_dev_id() {
  uint8_t raw = dip_read_raw();
  return raw & 0x3F; // bit0-5 = SW1-6 (gia dinh, xem ghi chu trong dipsw.h) - 0=Hub, 1-63=Slave
}

uint8_t dip_baud_sel() {
  uint8_t raw = dip_read_raw();
  return (raw >> 6) & 0x03; // bit6-7 = SW7-8
}

uint32_t dip_baud_value() {
  static const uint32_t table[4] = {4800, 9600, 14400, 19200};
  return table[dip_baud_sel()];
}

void dip_process() {
  static uint8_t last = 0xFF;
  static bool first = true;
  static uint32_t t_chg = 0;
  static uint8_t cand = 0xFF;

  uint8_t v = dip_read_raw();
  uint32_t now = millis();

  if (v != cand) {
    cand = v;
    t_chg = now;
  }
  if ((first || v != last) && (now - t_chg >= 30)) { // on dinh 30ms
    first = false;
    last = v;
    dbg_lock();
    SerialDBG.print("DIP: 0x");
    if (v < 0x10)
      SerialDBG.print("0");
    SerialDBG.print(v, HEX);
    SerialDBG.print(" DEVID=");
    SerialDBG.print(v & 0x3F);
    SerialDBG.print(" BAUD=");
    SerialDBG.println(dip_baud_value());
    dbg_unlock();
  }
}
