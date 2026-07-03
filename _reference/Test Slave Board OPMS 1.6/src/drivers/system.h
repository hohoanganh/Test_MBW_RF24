#pragma once
#include <Arduino.h>

// =====================================================================
//  System driver: cac thanh phan he thong tren board.
//   - Flash W25Q80 (SPI3)
//   - Version board (VER_CFG[2:0])
//   - RTC noi (se them sau - xem TODO)
// =====================================================================
void system_init();

// ---- Flash W25Q80 ----
bool flash_read_id(uint8_t *mfr, uint8_t *type, uint8_t *cap); // true neu Winbond

// ---- Version board ----
uint8_t board_version_read();            // 0..7 (3 = v1.6)

// ---- RTC noi (TODO: phat trien test RTC) ----
// bool rtc_selftest();                  // se hien thuc khi co thu vien/khai test
