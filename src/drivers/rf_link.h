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
//    (dev_id, seq) o dau nhan -> tang xac suat toi noi ma khong dung ACK.
// =========================================================

// 2026-07-06 (muc 3.1/3.3 tai lieu dinh huong): BO thuat toan tu tang/giam
// redundant-TX theo link toan mang - voi 16 slave cung 1 NET_ID, gan nhu giay
// nao hub cung nghe DUOC AI DO phat -> thuat toan luon thay "link tot", giam
// ve muc MIN va KHONG BAO GIO tang du phong cho dung slave xa/yeu dang rot
// nang (bi "trung binh hoa"). Nay dung GIA TRI CO DINH da kiem chung qua bench
// test (doi duoc luc chay bang CLI "rf redund <2-6>", chi RAM - de Modbus
// timeout/retry lop tren xu ly phan con lai (khuyen nghi timeout 1000-1500ms).
#define RF_REDUNDANT_TX_DEFAULT 3 // so lan gui lap lai moi khung (CO DINH, doi bang "rf redund")
#define RF_REDUNDANT_TX_MIN 2     // bien duoi cho "rf redund" (van can du phong toi thieu)
#define RF_REDUNDANT_TX_MAX 6     // bien tren cho "rf redund" (tranh chiem kenh qua nhieu)
#define RF_RELAY_TX 2             // so lan phat lai moi khung khi lam REPEATER (nhu heartbeat)

// ---------------------------------------------------------------------
// HEARTBEAT + GIAM SAT LINK (lay y tuong tu bo telemetry MAVLink/SiK radio:
// tu phat 1 khung dieu khien nho dinh ky de ben kia biet "con song", tu do
// phat hien LINK UP/DOWN va tu dieu chinh do du phong (redundant TX) theo
// chat luong duong truyen thuc te, thay vi dung 1 muc co dinh cho moi luc).
// ---------------------------------------------------------------------
#define RF_HB_PERIOD_MS 1000     // chu ky gui heartbeat + chu ky danh gia link
#define RF_LINK_TIMEOUT_MS 3000  // qua thoi gian nay ma khong nhan duoc GI (du lieu
                                 // hay heartbeat) tu dung dev_id -> bao LINK DOWN
#define RF_LINK_TIMEOUT_S (RF_LINK_TIMEOUT_MS / 1000) // ban theo dev_id dung don vi giay (xem ly do trong rf_link.cpp)

// 2026-07-04 (xem docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md muc 3.3, 9):
// thiet ke san cho toi da 64 thiet bi/mang tu dau (dev_id 0-63), du trien
// khai thuc te truoc mat chi 16 slave - tranh phai sua lai kich thuoc mang
// khi lap them slave sau nay.
//
// 2026-07-05: LAN DAU trien khai mang last_seen dang uint32_t millis() + 2
// mang bool[64] rieng (seen/link_up) TRAN RAM tren phan cung that (chip chi
// 10KB, bao "region RAM overflowed by 200 bytes" luc link). Da toi uu lai:
// - dedup KHONG con dung bang "slot + tim kiem tuyen tinh" nua (chi can thiet
//   khi khong gian dinh danh > so slot) - vi dev_id da BI CHAN CHINH XAC trong
//   0..63 == RF_MAX_DEV, dung THANG dev_id LAM CHI SO mang, bo hoan toan mang
//   luu "dev_id cua slot" (tiet kiem ~64 byte, code cung don gian hon).
// - 2 mang bool[64] (seen/link_up) doi thanh bitmap uint8_t[8] (1 bit/thiet
//   bi) - giam 64 byte -> 8 byte MOI mang.
// - last_seen doi tu millis() (uint32_t, 256 byte ca mang) sang "giay ke tu
//   boot" (uint16_t, 128 byte ca mang) - CHI dung cho hien thi CLI chan doan
//   (rf devices/rf dev), KHONG anh huong toi thuat toan redundant-TX toan cuc
//   (van dung millis() 32-bit nhu cu). Danh doi: wraparound moi ~18.2 gio
//   (65536s) thay vi ~49 ngay cua millis() - chap nhan duoc vi day la so lieu
//   chan doan cho ky thuat vien xem thu cong, khong phai vong dieu khien thoi
//   gian thuc; do phan giai giay (khong phai ms) van du dung so voi nguong
//   RF_LINK_TIMEOUT_MS=3000 (3 giay).
#define RF_MAX_DEV 64

void rf_init(uint8_t channel, uint8_t net_id, uint8_t dev_id);

bool rf_ok(); // radio.isChipConnected()

void rf_set_channel(uint8_t ch); // 0-125 (mac dinh nen dung 120-125, tranh WiFi)
uint8_t rf_get_channel();

void rf_set_network_id(uint8_t id); // doi dia chi pipe theo Network ID (0-63) - RAM/session, khong tu luu Flash (xem flashmem.h/net_id_save() de luu)
uint8_t rf_get_network_id();

uint8_t rf_get_dev_id(); // dev_id cua CHINH board nay (0=Hub, 1-63=Slave, tu DIP - xem dipsw.h)

// Gui 1 ban tin ung dung (co the > 25 byte, se tu chia manh). Tra ve false
// neu ban tin qua dai (> RF_CHUNK_MAX * RF_MAX_FRAG).
bool rf_send(const uint8_t *data, uint16_t len);

bool rf_available();          // co 1 ban tin da ghep day du dang cho
uint16_t rf_read(uint8_t *buf, uint16_t bufsize); // tra ve so byte, 0 neu khong co

void rf_process(); // goi trong loop(): bom nhan/ghep manh, cap nhat thong ke

void rf_get_stats(uint32_t *tx, uint32_t *rx_ok, uint32_t *rx_dup,
                   uint32_t *rx_crcerr, uint32_t *rx_fragdrop);
void rf_reset_stats();

// ----- Heartbeat / Link health TOAN MANG (goi rf_process() moi vong loop() la
// du, ham nay tu chay ben trong rf_process(), khong can goi rieng). 2026-07-06:
// redundant-TX da chuyen sang GIA TRI CO DINH (xem ghi chu RF_REDUNDANT_TX_
// DEFAULT o tren) - cac getter/counter duoi day chi con phuc vu chan doan. -----
bool rf_link_up();              // true = da nhan duoc gi do (tu BAT KY dev_id nao)
                                 // trong vong RF_LINK_TIMEOUT_MS gan nhat
uint8_t rf_link_peer_id();      // dev_id da nghe thay GAN NHAT (khong dai dien ca mang)
uint32_t rf_link_age_ms();      // so ms tu lan nhan cuoi cung (bat ky khung nao, bat ky dev_id)
uint8_t rf_get_redundancy();    // so lan gui lap hien tai (CO DINH, doi bang rf_set_redundancy)
void rf_set_redundancy(uint8_t n); // doi so lan gui lap (kep trong MIN..MAX) - CLI "rf redund <n>", chi RAM
uint16_t rf_get_loss_permille(); // uoc luong ty le "ky heartbeat bi mat trang" TOAN MANG (phan nghin)
void rf_get_hb_stats(uint32_t *hb_tx, uint32_t *hb_rx);

// ----- REPEATER 1-hop (muc 6 tai lieu dinh huong) - relay DOI XUNG ca 2 chieu
// (poll hub->slave lan response slave->hub), giu nguyen dev_id/seq goc (ben
// nhan coi nhu 1 lan "gui lap" binh thuong, dedup san co tu loc), chi relay
// BAN DAU TIEN cua moi (dev_id,seq) va chi khi hop > 0 (giam 1 moi lan relay).
// Chi nen bat cho DUNG 1 board dat giua hub va (cac) slave xa - KHONG bat tren
// moi slave (broadcast storm). Bat/tat: CLI "rf repeater on|off" (luu Flash,
// xem flashmem.h) hoac giu nut S2 3 giay (xem ledbuzz.cpp). -----
void rf_set_repeater(bool en); // chi RAM - persist do CLI/nut S2 tu goi repeater_save()
bool rf_is_repeater();
uint32_t rf_get_relay_cnt();   // so khung DA relay (moi (dev_id,seq) tinh 1, khong tinh so lan phat lai)

// ----- Heartbeat / Link health THEO TUNG dev_id (moi 3.3/7.1) - phuc vu chan
// doan dung thiet bi nao dang mat song khi co N slave dung chung 1 NET_ID,
// thay vi chi 1 trang thai UP/DOWN gop chung ca mang o tren. -----
bool rf_dev_seen(uint8_t dev_id);      // da tung nhan duoc khung hop le nao tu dev_id nay chua
bool rf_dev_link_up(uint8_t dev_id);   // true = nhan duoc gi do tu dev_id nay trong RF_LINK_TIMEOUT_MS gan nhat
uint32_t rf_dev_age_s(uint8_t dev_id); // so GIAY (khong phai ms - xem ghi chu RF_MAX_DEV o tren) tu lan
                                       // nhan cuoi tu dev_id nay (UINT32_MAX neu chua tung thay)

// LOSS%o THEO TUNG dev_id (2026-07-06, muc 7.2): moi thiet bi phat heartbeat
// 1 lan/giay -> so heartbeat NHAN DUOC tu 1 dev_id so voi so giay troi qua ke
// tu lan "rf reset" gan nhat cho ra ty le mat rieng cua thiet bi do (phan
// nghin, 0-1000). QUY TRINH DO DUNG: cap nguon du moi board -> "rf reset" ->
// cho >= 60 giay -> "rf devices" (cot loss%o). Thiet bi len nguon SAU lan
// "rf reset" se bi tinh loss cao gia tao cho toi lan reset ke tiep. Cua so do
// toi da ~18.2 gio (uint16 giay) - du cho khao sat T3/T4, khong phai so lieu
// tich luy vinh vien. Tra ve 0xFFFF neu chua du du lieu (< 10 giay hoac chua
// tung thay dev_id nay).
uint16_t rf_dev_loss_permille(uint8_t dev_id);
