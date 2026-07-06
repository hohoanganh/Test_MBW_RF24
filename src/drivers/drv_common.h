#pragma once
#include <Arduino.h>

// =========================================================
// Hang so + tien ich DUNG CHUNG cho rf_link.cpp va bridge.cpp
// =========================================================

// ----- Khung tin RF co dinh 32 byte (KHONG dung dynamic payload: it phu
// thuoc "bat tay" giua 2 dau, on dinh hon trong moi truong nhieu) -----
#define RF_PAYLOAD_SIZE 32
#define RF_HDR_LEN 6    // dev_id(1) + seq(1) + frag_idx(1) + frag_total(1) + len(1) + hop(1)
#define RF_CRC_LEN 2    // CRC16 cuoi khung
#define RF_CHUNK_MAX (RF_PAYLOAD_SIZE - RF_HDR_LEN - RF_CRC_LEN) // = 24 byte du lieu/khung

// 2026-07-06 (muc 6 tai lieu dinh huong): them 1 byte hop-count/TTL phuc vu
// REPEATER 1-HOP cho slave o xa/khuat hub. Node goc phat voi hop =
// RF_HOP_DEFAULT; board bat che do repeater (CLI "rf repeater on" hoac giu
// nut S2 3 giay) khi nghe duoc khung cua node KHAC voi hop > 0 se phat lai
// DUNG 1 LAN (bản đầu tiên của mỗi (dev_id,seq)) sau khi giam hop di 1 -
// het hop thi khong relay tiep, tranh lap vo han khi nhieu repeater nghe nhau.
// LUU Y: doi cau truc khung -> MOI board trong cung 1 mang PHAI nap cung
// phien ban firmware nay (khung cu 5-byte header se bi loai boi CRC).
#define RF_HOP_DEFAULT 1 // toi da 1 lan relay (repeater 1-hop, khong mesh)

// 2026-07-04 (xem docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md muc 3): TRUOC
// DAY truong dau tien la "src_id" duoc gan BANG Network ID (moi node trong 1
// NET_ID broadcast domain deu bao cung 1 gia tri) - sai khi co N>1 slave dung
// chung 1 NET_ID (16 hien tai, toi da 64 sau nay): dedup/heartbeat khong the
// phan biet "ai gui". Nay doi thanh "dev_id" - ID RIENG cho TUNG BOARD trong
// mang (0 = Hub, 1-63 = Slave, set qua DIP SW1-6, xem dipsw.h), DOC LAP voi
// dia chi Modbus that. NET_ID (0-63, cau hinh qua CLI "net id" + luu Flash,
// xem flashmem.h) khong con nam trong payload nua vi da an san trong chinh
// dia chi pipe RF - moi khung nhan duoc tren 1 pipe chac chan cung NET_ID.
#pragma pack(push, 1)
typedef struct {
  uint8_t dev_id;       // ID thiet bi RF (0-63): 0=Hub, 1-63=Slave - DOC LAP voi dia chi Modbus
  uint8_t seq;          // so thu tu tang dan (theo tung dev_id) - dedup phia nhan
  uint8_t frag_idx;     // manh thu i (bat dau tu 0) - khi 1 khung Modbus > 24 byte
  uint8_t frag_total;   // tong so manh cua khung Modbus nay
  uint8_t len;          // so byte du lieu thuc trong payload[] (<= RF_CHUNK_MAX)
  uint8_t hop;          // hop-count/TTL con lai (repeater giam 1 moi lan relay, 0 = khong relay nua)
  uint8_t payload[RF_CHUNK_MAX];
  uint16_t crc16;       // CRC16-MODBUS tren (header + payload[0..len-1]) - repeater TINH LAI sau khi giam hop
} rf_frame_t;
#pragma pack(pop)

// CRC16-MODBUS (poly 0xA001, init 0xFFFF) - dung chung de kiem tra toan ven
// khung RF va co the doi chieu voi CRC cuoi khung Modbus RTU that (khong bat
// buoc, chi ho tro do "khoang lang" giua 2 khung Modbus sau nay neu can).
uint16_t crc16_modbus(const uint8_t *data, uint16_t len);
