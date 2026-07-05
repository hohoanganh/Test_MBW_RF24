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
#define NET_ID_FLASH_ADDR 0x001000 // sector 1 (4KB), tach voi sector test 0x0FF000
#define NET_ID_UNSET 0xFF          // gia tri doc duoc khi CHUA tung ghi (flash trong)

void net_id_save(uint8_t id);  // ghi 1 byte NET_ID (0-63) xuong Flash (tu xoa sector truoc)
uint8_t net_id_load();         // doc NET_ID tu Flash; tra ve NET_ID_UNSET neu chua cau hinh
