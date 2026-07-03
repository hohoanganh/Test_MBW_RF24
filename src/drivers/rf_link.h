#pragma once
#include <Arduino.h>

// =========================================================
// nRF24L01P + RFX2401C (PA/LNA) - lien ket khong day 2.4GHz
// Dung thu vien RF24 (TMRh20) cho tang SPI/protocol (da toi uu, it bug hon tu
// viet driver thap cap). TXEN/RXEN cua RFX2401C dau cung PA8/RF_CE tren
// schematic -> PA/LNA tu chuyen mach theo CE, khong can GPIO rieng.
//
// THIET KE CHO MOI TRUONG NHIEU:
//  - 250kbps (nhay hon 1/2Mbps, chiu nhieu tot hon, van du nhanh cho RS485
//    4800-19200 baud von da cham hon nhieu).
//  - PA_MAX + RFX2401C khuech dai them tam phat/thu.
//  - CRC16 phan cung (nRF24) + CRC16-MODBUS rieng cua ung dung (drv_common.h)
//    -> 2 lop kiem tra toan ven, loai bo goi tin loi mem con sot khi nhieu.
//  - Payload CO DINH 32 byte (khong dung dynamic payload) -> giam mot lop
//    "bat tay" co the that bai khi song yeu.
//  - KHONG dung auto-ack/auto-retry phan cung: day la kenh BROADCAST (nhieu
//    thiet bi cung Network ID cung nghe 1 dia chi) nen ACK 1-1 khong hop le.
//    Thay vao do: gui LAP LAI moi khung (REDUNDANT_TX lan) + loc trung bang
//    (src_id, seq) o dau nhan -> tang xac suat toi noi ma khong dung ACK.
// =========================================================

#define RF_REDUNDANT_TX_DEFAULT 3 // so lan gui lap lai moi khung (gia tri khoi dong)
#define RF_REDUNDANT_TX_MIN 2     // khong giam duoi muc nay (van can du phong toi thieu)
#define RF_REDUNDANT_TX_MAX 6     // khong tang qua muc nay (tranh chiem kenh qua nhieu)

// ---------------------------------------------------------------------
// HEARTBEAT + GIAM SAT LINK (lay y tuong tu bo telemetry MAVLink/SiK radio:
// tu phat 1 khung dieu khien nho dinh ky de ben kia biet "con song", tu do
// phat hien LINK UP/DOWN va tu dieu chinh do du phong (redundant TX) theo
// chat luong duong truyen thuc te, thay vi dung 1 muc co dinh cho moi luc).
// ---------------------------------------------------------------------
#define RF_HB_PERIOD_MS 1000     // chu ky gui heartbeat + chu ky danh gia link
#define RF_LINK_TIMEOUT_MS 3000  // qua thoi gian nay ma khong nhan duoc GI (du lieu
                                 // hay heartbeat) tu ben kia -> bao LINK DOWN

void rf_init(uint8_t channel, uint8_t network_id);

bool rf_ok(); // radio.isChipConnected()

void rf_set_channel(uint8_t ch); // 0-125 (mac dinh nen dung 120-125, tranh WiFi)
uint8_t rf_get_channel();

void rf_set_network_id(uint8_t id); // doi dia chi pipe theo Network ID (0-63)
uint8_t rf_get_network_id();

// Gui 1 ban tin ung dung (co the > 25 byte, se tu chia manh). Tra ve false
// neu ban tin qua dai (> RF_CHUNK_MAX * RF_MAX_FRAG).
bool rf_send(const uint8_t *data, uint16_t len);

bool rf_available();          // co 1 ban tin da ghep day du dang cho
uint16_t rf_read(uint8_t *buf, uint16_t bufsize); // tra ve so byte, 0 neu khong co

void rf_process(); // goi trong loop(): bom nhan/ghep manh, cap nhat thong ke

void rf_get_stats(uint32_t *tx, uint32_t *rx_ok, uint32_t *rx_dup,
                   uint32_t *rx_crcerr, uint32_t *rx_fragdrop);
void rf_reset_stats();

// ----- Heartbeat / Link health (goi rf_process() moi vong loop() la du, ham
// nay tu chay ben trong rf_process(), khong can goi rieng) -----
bool rf_link_up();              // true = da nhan duoc gi do (du lieu/heartbeat)
                                 // tu ben kia trong vong RF_LINK_TIMEOUT_MS gan nhat
uint8_t rf_link_peer_id();      // src_id cua ben da nghe thay gan nhat
uint32_t rf_link_age_ms();      // so ms tu lan nhan cuoi cung (bat ky khung nao)
uint8_t rf_get_redundancy();    // so lan gui lap hien tai (tu dieu chinh RF_REDUNDANT_TX_MIN..MAX)
uint16_t rf_get_loss_permille(); // uoc luong ty le "ky heartbeat bi mat trang" (phan nghin, 0-1000)
void rf_get_hb_stats(uint32_t *hb_tx, uint32_t *hb_rx);
