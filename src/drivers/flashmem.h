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
