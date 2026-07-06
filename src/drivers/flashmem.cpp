#include "flashmem.h"
#include "../mbw_common.h"
#include "../rtos_glue.h"
#include <string.h>

// g_muSPI bao ve bus SPI1 dung chung voi nRF24L01 (rf_link.cpp) - xem giai
// thich chi tiet + QUY TAC THU TU KHOA (tranh deadlock voi g_muSerial) trong
// rtos_glue.h va rf_link.cpp. O FILE NAY: cac HAM CONG KHAI tu khoa/mo rieng
// (self-contained); KHONG ham nao goi 1 ham cong khai KHAC cua chinh file nay
// trong luc dang giu khoa (mutex khong dung kieu recursive - se treo).

#define CMD_WREN 0x06
#define CMD_RDSR 0x05
#define CMD_PP 0x02
#define CMD_READ 0x03
#define CMD_SE 0x20 // sector erase 4KB
#define CMD_JEDEC_ID 0x9F

void flash_init() {
  pinMode(FLASH_CS, OUTPUT);
  digitalWrite(FLASH_CS, HIGH);
}

static void flash_wren() {
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(CMD_WREN);
  digitalWrite(FLASH_CS, HIGH);
}

static uint8_t flash_rdsr() {
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(CMD_RDSR);
  uint8_t s = SPI.transfer(0);
  digitalWrite(FLASH_CS, HIGH);
  return s;
}

static void flash_wait() {
  while (flash_rdsr() & 0x01)
    delay(1);
}

static void flash_send_addr(uint32_t addr) {
  SPI.transfer((uint8_t)(addr >> 16));
  SPI.transfer((uint8_t)(addr >> 8));
  SPI.transfer((uint8_t)addr);
}

bool flash_read_id(uint8_t *mfg, uint8_t *memtype, uint8_t *cap) {
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(CMD_JEDEC_ID);
  *mfg = SPI.transfer(0);
  *memtype = SPI.transfer(0);
  *cap = SPI.transfer(0);
  digitalWrite(FLASH_CS, HIGH);
  xSemaphoreGive(g_muSPI);
  return (*mfg == 0xEF); // Winbond
}

void flash_print_id() {
  uint8_t m, t, c;
  // flash_read_id() tu khoa/mo g_muSPI rieng - KHONG khoa them o day (tranh
  // khoa long nhau tren cung 1 mutex khong-recursive).
  bool ok = flash_read_id(&m, &t, &c);
  dbg_lock();
  SerialDBG.print("FLASH: ");
  SerialDBG.print(m, HEX);
  SerialDBG.print(" ");
  SerialDBG.print(t, HEX);
  SerialDBG.print(" ");
  SerialDBG.println(c, HEX);
  SerialDBG.println(ok ? "FLASH_ID=OK" : "FLASH_ID=FAIL");
  dbg_unlock();
}

static void flash_erase_sector(uint32_t addr) {
  flash_wren();
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(CMD_SE);
  flash_send_addr(addr);
  digitalWrite(FLASH_CS, HIGH);
  flash_wait();
}

static void flash_write(uint32_t addr, const uint8_t *data, uint16_t len) {
  flash_wren();
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(CMD_PP);
  flash_send_addr(addr);
  for (uint16_t i = 0; i < len; i++)
    SPI.transfer(data[i]);
  digitalWrite(FLASH_CS, HIGH);
  flash_wait();
}

static void flash_read(uint32_t addr, uint8_t *buf, uint16_t len) {
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(CMD_READ);
  flash_send_addr(addr);
  for (uint16_t i = 0; i < len; i++)
    buf[i] = SPI.transfer(0);
  digitalWrite(FLASH_CS, HIGH);
}

void flash_read_bytes(uint32_t addr, uint8_t *buf, uint16_t len) {
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  flash_read(addr, buf, len);
  xSemaphoreGive(g_muSPI);
}
void flash_write_bytes(uint32_t addr, const uint8_t *data, uint16_t len) {
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  flash_erase_sector(addr); // W25Q128 sector = 4KB, xoa truoc khi ghi
  flash_write(addr, data, len);
  xSemaphoreGive(g_muSPI);
}
void flash_erase_sector_at(uint32_t addr) {
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  flash_erase_sector(addr);
  xSemaphoreGive(g_muSPI);
}

void flash_test_rw() {
  dbg_lock();
  SerialDBG.println("FLASH RW TEST");
  dbg_unlock();

  uint8_t tx[16], rx[16];
  uint8_t seed = (uint8_t)millis();

  for (uint16_t i = 0; i < 16; i++) {
    tx[i] = i ^ seed;
    rx[i] = 0;
  }

  // Dung sector cuoi cung de test, tranh dam vao vung du lieu firmware co
  // the dung sau nay (addr 0x00 de lai lam vung du lieu ung dung).
  const uint32_t TEST_ADDR = 0x0FF000; // sector gan cuoi trong 16MB
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  flash_erase_sector(TEST_ADDR);
  flash_write(TEST_ADDR, tx, 16);
  flash_read(TEST_ADDR, rx, 16);
  xSemaphoreGive(g_muSPI);

  dbg_lock();
  SerialDBG.println(memcmp(tx, rx, 16) == 0 ? "FLASH_RW=OK" : "FLASH_RW=FAIL");
  dbg_unlock();
}

// ----- Cau hinh persist: byte0 = NET_ID, byte1 = co REPEATER (xem giai thich
// trong flashmem.h). Read-modify-write ca 2 byte de save cai nay khong lam
// mat cai kia (flash_write_bytes() xoa nguyen sector 4KB truoc khi ghi). -----
static void cfg_save_byte(uint8_t offset, uint8_t value) {
  uint8_t cfg[2];
  flash_read_bytes(CFG_FLASH_ADDR, cfg, 2); // tu khoa/mo g_muSPI rieng
  cfg[offset] = value;
  flash_write_bytes(CFG_FLASH_ADDR, cfg, 2); // tu xoa sector + tu khoa/mo g_muSPI
}

void net_id_save(uint8_t id) { cfg_save_byte(0, id); }

uint8_t net_id_load() {
  uint8_t id = NET_ID_UNSET;
  flash_read_bytes(CFG_FLASH_ADDR, &id, 1);
  return id;
}

void repeater_save(bool en) { cfg_save_byte(1, en ? 1 : 0); }

bool repeater_load() {
  uint8_t cfg[2] = {0xFF, 0xFF};
  flash_read_bytes(CFG_FLASH_ADDR, cfg, 2);
  return cfg[1] == 1; // 0xFF (chua cau hinh) va 0 deu la OFF
}

// ================= EVENT LOG mat-link (xem flashmem.h) =================
// KHONG dung RAM tinh (chip chi 10KB) - vi tri ghi quet ON-DEMAND moi lan.
#define LOG_BASE 0x010000UL // vung log, cach xa CFG(0x001000) + test(0x0FF000)
#define LOG_SECTORS 4       // 4 x 4KB = 16KB
#define LOG_CAP ((LOG_SECTORS * 4096) / FLASHLOG_REC_SIZE) // 2048 ban ghi

// Tim vi tri ghi ke tiep = ban ghi dau tien co byte0 == 0xFF (flash trong).
// CALLER PHAI dang giu g_muSPI (dung flash_read static). Tra ve LOG_CAP neu day.
static uint32_t log_find_widx() {
  for (uint32_t i = 0; i < LOG_CAP; i++) {
    uint8_t b0;
    flash_read(LOG_BASE + i * FLASHLOG_REC_SIZE, &b0, 1);
    if (b0 == 0xFF)
      return i;
  }
  return LOG_CAP; // day
}

// Ghi 1 ban ghi (page program vao byte 0xFF con trong). Day thi vong lai (xoa
// toan vung, ghi tu dau). PHAI goi TU RF task o NGOAI vung khoa g_muSPI.
void flashlog_append(uint8_t hh, uint8_t mm, uint8_t ss, uint8_t dd, uint8_t mo,
                     uint8_t dev_id, uint8_t evt, uint8_t loss_pct) {
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  uint32_t widx = log_find_widx();
  if (widx >= LOG_CAP) { // day -> vong lai (mat lich su cu)
    for (uint8_t s = 0; s < LOG_SECTORS; s++)
      flash_erase_sector(LOG_BASE + (uint32_t)s * 4096);
    widx = 0;
  }
  uint8_t rec[FLASHLOG_REC_SIZE] = {hh, mm, ss, dev_id, evt, loss_pct, dd, mo};
  flash_write(LOG_BASE + widx * FLASHLOG_REC_SIZE, rec, FLASHLOG_REC_SIZE);
  xSemaphoreGive(g_muSPI);
}

uint16_t flashlog_count() {
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  uint16_t n = (uint16_t)log_find_widx();
  xSemaphoreGive(g_muSPI);
  return n;
}

bool flashlog_read(uint16_t idx, uint8_t rec[FLASHLOG_REC_SIZE]) {
  if (idx >= LOG_CAP)
    return false;
  flash_read_bytes(LOG_BASE + (uint32_t)idx * FLASHLOG_REC_SIZE, rec, FLASHLOG_REC_SIZE);
  return rec[0] != 0xFF; // 0xFF = slot trong
}

void flashlog_clear() {
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  for (uint8_t s = 0; s < LOG_SECTORS; s++)
    flash_erase_sector(LOG_BASE + (uint32_t)s * 4096);
  xSemaphoreGive(g_muSPI);
}
