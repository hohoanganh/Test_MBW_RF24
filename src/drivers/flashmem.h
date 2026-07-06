#pragma once
#include <Arduino.h>

// W25Q128JVSIQ (128Mbit = 16MB) tren SPI1, CS = PB14. SPI1 dung CHUNG bus voi
// nRF24L01 (rf_link.cpp) - chi truy cap tuan tu trong vong lap chinh (khong
// dung ngat/RTOS xen giua 2 thiet bi), an toan vi main.cpp la single-thread.

void flash_init();

void flash_print_id();          // in "FLASH: <mfg> <type> <cap>" + FLASH_ID_OK/FAIL
bool flash_read_id(uint8_t *mfg, uint8_t *memtype, uint8_t *cap);
void flash_test_rw();           // ghi/doc thu sector 0, in FLASH_RW=OK/FAIL

void flash_read_bytes(uint32_t addr, uint8_t *buf, uint16_t len);
void flash_write_bytes(uint32_t addr, const uint8_t *data, uint16_t len); // tu xoa sector truoc khi ghi
void flash_erase_sector_at(uint32_t addr);

// ===== NET_ID persist (2026-07-04, xem docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md
// muc 3.2.b): NET_ID la hang so CHUNG cho ca 1 deployment (hub + moi slave
// cung 1 gia tri) - khong con doc tu DIP switch (SW1-6 nay danh cho DEV_ID,
// xem dipsw.h), ma cau hinh 1 lan qua CLI "net id <n>" va luu vao Flash de
// giu qua cac lan mat nguon. Dung 1 sector rieng (cach xa sector test
// 0x0FF000 cua flash_test_rw()) - flash da xoa (chua ghi) doc ra toan 0xFF,
// TRUNG luon voi sentinel "chua cau hinh" nen khong can them co/byte danh dau
// rieng. =====
// 2026-07-06: sector cau hinh nay gio chua 2 byte (cung sector vi flash chi
// xoa duoc theo nguyen sector 4KB - luu roi rac 2 sector se ton them 4KB va 2
// lan erase): byte0 = NET_ID, byte1 = co REPEATER (muc 6 tai lieu dinh huong).
// Moi ham save doc ca 2 byte len truoc -> sua dung byte cua minh -> xoa sector
// -> ghi lai ca 2 (read-modify-write), nen set NET_ID khong lam mat co
// repeater va nguoc lai.
#define CFG_FLASH_ADDR 0x001000    // sector 1 (4KB), tach voi sector test 0x0FF000
#define NET_ID_FLASH_ADDR CFG_FLASH_ADDR // giu ten cu cho tuong thich (byte0)
#define NET_ID_UNSET 0xFF          // gia tri doc duoc khi CHUA tung ghi (flash trong)

void net_id_save(uint8_t id);  // ghi NET_ID (0-63) xuong byte0 (giu nguyen byte co repeater)
uint8_t net_id_load();         // doc NET_ID tu Flash; tra ve NET_ID_UNSET neu chua cau hinh

// Co REPEATER 1-hop (byte1): 0xFF (flash trong, chua cau hinh) = OFF, 0 = OFF,
// 1 = ON. Luu qua cac lan mat nguon - board dat lam repeater giu vai tro sau
// khi mat dien ma khong can cau hinh lai.
void repeater_save(bool en);   // ghi co repeater xuong byte1 (giu nguyen NET_ID)
bool repeater_load();          // doc co repeater; false neu chua tung cau hinh

// ===== EVENT LOG mat-link theo dev_id, luu Flash (2026-07-06) =====
// Ghi su kien MAT LINK (DOWN) cua tung dev_id kem gio RTC + loss% luc do, giu
// qua mat nguon -> chan doan "dem qua con nao rot, luc may gio". Vung rieng
// 0x010000 (4 sector = 16KB = 2048 ban ghi 8-byte), cach xa CFG (0x001000) va
// test (0x0FF000). Ghi noi tiep (page program), day thi vong lai. Ban ghi 8 byte:
//   [0]=hh [1]=mm [2]=ss [3]=dev_id [4]=evt(0=DOWN) [5]=loss%(0xFF=chua ro) [6]=day [7]=month
// KHONG dung RAM tinh (vi tri ghi quet on-demand) vi chip chi 10KB RAM. Goi
// flashlog_append() TU RF TASK, o ngoai vung khoa g_muSPI (rf_heartbeat_tick).
#define FLASHLOG_EVT_DOWN 0
#define FLASHLOG_EVT_UP   1
#define FLASHLOG_REC_SIZE 8

void flashlog_append(uint8_t hh, uint8_t mm, uint8_t ss, uint8_t dd, uint8_t mo,
                     uint8_t dev_id, uint8_t evt, uint8_t loss_pct); // ghi 1 su kien
uint16_t flashlog_count();      // so ban ghi hien co
bool flashlog_read(uint16_t idx, uint8_t rec[FLASHLOG_REC_SIZE]); // false neu idx trong/vuot
void flashlog_clear();          // xoa toan bo log
