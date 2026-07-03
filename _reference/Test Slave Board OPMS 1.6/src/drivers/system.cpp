#include "../opms_common.h"
#include "system.h"
#include <SPI.h>

// SPI3 cho W25Q80 (MOSI=PB5, MISO=PB4, SCK=PB3)
static SPIClass SPI_FLASH(SPI_MOSI, SPI_MISO, SPI_SCK);

void system_init() {
  pinMode(FLASH_CS, OUTPUT); digitalWrite(FLASH_CS, HIGH);
  SPI_FLASH.begin();
  // TODO: rtc_init() khi phat trien test RTC noi.
}

// ---- Flash W25Q80 (JEDEC ID 0x9F) ----
bool flash_read_id(uint8_t *mfr, uint8_t *type, uint8_t *cap) {
  SPI_FLASH.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(FLASH_CS, LOW);
  SPI_FLASH.transfer(0x9F);
  uint8_t m = SPI_FLASH.transfer(0x00);
  uint8_t t = SPI_FLASH.transfer(0x00);
  uint8_t c = SPI_FLASH.transfer(0x00);
  digitalWrite(FLASH_CS, HIGH);
  SPI_FLASH.endTransaction();
  if (mfr)  *mfr = m;
  if (type) *type = t;
  if (cap)  *cap = c;
  return (m == 0xEF);    // 0xEF = Winbond (W25Qxx)
}

// ---- Version board ----
uint8_t board_version_read() {
  pinMode(VER_CFG0, INPUT_PULLUP);
  pinMode(VER_CFG1, INPUT_PULLUP);
  pinMode(VER_CFG2, INPUT_PULLUP);
  uint8_t b0 = digitalRead(VER_CFG0) ? 1 : 0;
  uint8_t b1 = digitalRead(VER_CFG1) ? 1 : 0;
  uint8_t b2 = digitalRead(VER_CFG2) ? 1 : 0;
  return (uint8_t)((b2 << 2) | (b1 << 1) | b0);
}
