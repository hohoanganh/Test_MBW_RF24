#pragma once
#include <Arduino.h>

// =========================================================
// Hang so + tien ich DUNG CHUNG cho rf_link.cpp va bridge.cpp
// =========================================================

// ----- Khung tin RF co dinh 32 byte (KHONG dung dynamic payload: it phu
// thuoc "bat tay" giua 2 dau, on dinh hon trong moi truong nhieu) -----
#define RF_PAYLOAD_SIZE 32
#define RF_HDR_LEN 5    // src_id(1) + seq(1) + frag_idx(1) + frag_total(1) + len(1)
#define RF_CRC_LEN 2    // CRC16 cuoi khung
#define RF_CHUNK_MAX (RF_PAYLOAD_SIZE - RF_HDR_LEN - RF_CRC_LEN) // = 25 byte du lieu/khung

#pragma pack(push, 1)
typedef struct {
  uint8_t src_id;      // ID nguon (0-63, lay tu DIP Network ID) - chong lap/loc trung
  uint8_t seq;          // so thu tu tang dan (theo tung nguon) - dedup phia nhan
  uint8_t frag_idx;     // manh thu i (bat dau tu 0) - khi 1 khung Modbus > 25 byte
  uint8_t frag_total;   // tong so manh cua khung Modbus nay
  uint8_t len;          // so byte du lieu thuc trong payload[] (<= RF_CHUNK_MAX)
  uint8_t payload[RF_CHUNK_MAX];
  uint16_t crc16;       // CRC16-MODBUS tren (header + payload[0..len-1])
} rf_frame_t;
#pragma pack(pop)

// CRC16-MODBUS (poly 0xA001, init 0xFFFF) - dung chung de kiem tra toan ven
// khung RF va co the doi chieu voi CRC cuoi khung Modbus RTU that (khong bat
// buoc, chi ho tro do "khoang lang" giua 2 khung Modbus sau nay neu can).
uint16_t crc16_modbus(const uint8_t *data, uint16_t len);
