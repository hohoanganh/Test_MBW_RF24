#include "rf_link.h"
#include "drv_common.h"
#include "../mbw_common.h"
#include "../rtos_glue.h"
#include <RF24.h>
#include <string.h>

// g_muSPI bao ve bus SPI1 dung chung voi Flash (flashmem.cpp) - can thiet vi
// gio chay THAT SU song song tren nhieu task RTOS (RF task, CLI task), khac
// voi loop() don luong truoc day. Chi khoa o CAC HAM DUOC GOI TU BEN NGOAI
// (public) dung tren SPI - cac ham static noi bo (send_one_frame, apply_address...)
// luon duoc goi TU BEN TRONG 1 ham cong khai da khoa san, khong khoa long nhau.

#define RF_MAX_FRAG 10 // toi da 10 manh x 25 byte = 250 byte ~ Modbus RTU max thuc te

static RF24 radio(RF_CE, RF_CSN);

static uint8_t s_channel = 120; // mac dinh: dai tren cung ISM 2.4G, tranh het kenh WiFi 1-11
static uint8_t s_netid = 0;
static uint8_t s_dev_id = 0; // dev_id RIENG cua board nay (0=Hub, 1-63=Slave) - xem drv_common.h/dipsw.h
static uint8_t s_tx_seq = 0;

// ----- Thong ke -----
static uint32_t s_tx_cnt = 0, s_rx_ok_cnt = 0, s_rx_dup_cnt = 0, s_rx_crcerr_cnt = 0, s_rx_fragdrop_cnt = 0;

// ----- Heartbeat / Link health -----
static uint8_t s_redundant_tx = RF_REDUNDANT_TX_DEFAULT;
static uint32_t s_hb_last_tx_ms = 0;
static uint32_t s_last_rx_ms = 0;       // lan cuoi nhan duoc BAT KY khung hop le nao
static bool s_link_up = false;
static uint8_t s_peer_id = 0;
static uint32_t s_hb_tx_cnt = 0, s_hb_rx_cnt = 0;

// Danh gia chat luong theo tung "ky" RF_HB_PERIOD_MS: ky nao khong nhan duoc
// gi tinh la 1 lan "mat trang" -> dung de vua uoc luong ty le mat, vua lam
// dieu kien tang/giam so lan gui lap (redundant TX) tu dong.
static uint32_t s_period_start_ms = 0;
static bool s_period_had_rx = false;
static uint32_t s_period_ok_cnt = 0, s_period_miss_cnt = 0;
static uint8_t s_period_ok_streak = 0, s_period_miss_streak = 0;
#define RF_ADAPT_MISS_STREAK 2  // mat lien tiep 2 ky -> tang do du phong
#define RF_ADAPT_OK_STREAK 5    // tot lien tiep 5 ky -> giam do du phong

// ----- Heartbeat / Link health THEO TUNG dev_id (2026-07-04/05, muc 3.3/7.1) -----
// Mang CO DINH cho CA 64 dev_id (RF_MAX_DEV) tu dau, du hien tai chi 16 slave
// dang hoat dong - xem giai thich toi uu RAM (bitmap + giay thay vi ms) trong
// rf_link.h dau dinh nghia RF_MAX_DEV. Chi phi RAM: 64*2 (last_seen_s) + 8
// (link_up bitmap) = 136 byte (so voi 384 byte cua ban dau dung bool[64]).
static uint16_t s_dev_last_seen_s[RF_MAX_DEV]; // giay ke tu boot, 0 = CHUA TUNG thay dev_id nay
#define DEV_BITMAP_BYTES ((RF_MAX_DEV + 7) / 8)
static uint8_t s_dev_link_up_bitmap[DEV_BITMAP_BYTES]; // bit=1: dev_id dang UP

static inline bool dev_bitmap_get(const uint8_t *bm, uint8_t dev_id) {
  return (bm[dev_id >> 3] & (uint8_t)(1u << (dev_id & 7))) != 0;
}
static inline void dev_bitmap_set(uint8_t *bm, uint8_t dev_id, bool v) {
  if (v)
    bm[dev_id >> 3] |= (uint8_t)(1u << (dev_id & 7));
  else
    bm[dev_id >> 3] &= (uint8_t) ~(1u << (dev_id & 7));
}

// ----- Dedup (dev_id -> seq cuoi da nhan) -----
// 2026-07-04 (muc 3.1, 3.3, 8): TRUOC DAY khoa theo src_id = Network ID nen
// TOAN BO N thiet bi cung 1 NET_ID chi co dung 1 slot chung -> seq trung ngau
// nhien giua 2 thiet bi khac nhau (moi thiet bi tu dem seq doc lap 0-255) lam
// dedup NHAM tuong "trung" va am tham loai bo frame that. Nay khoa theo dev_id
// RIENG cho tung thiet bi (dev_id da tach khoi NET_ID - xem drv_common.h) nen
// moi thiet bi co 1 slot dedup rieng, khong con dam vao nhau nua.
//
// 2026-07-05: KHONG con dung bang "N slot + tim kiem tuyen tinh/round-robin"
// (kieu do CHI can thiet khi khong gian dinh danh nguon LON HON so slot san
// co). O day dev_id da bi CHAN CHINH XAC trong 0..63 == RF_MAX_DEV, nen dung
// THANG dev_id lam CHI SO mang - O(1), khong bao gio phai "chiem cho"/ghi de,
// va khong can luu them "dev_id cua slot nay la gi" (tiet kiem them RAM).
static uint8_t s_dev_last_seq[RF_MAX_DEV]; // seq cuoi da nhan tu dev_id nay
static uint8_t s_dev_seq_known_bitmap[DEV_BITMAP_BYTES]; // bit=1: da tung nhan seq nao tu dev_id nay

static bool dedup_check_and_update(uint8_t dev_id, uint8_t seq) {
  if (dev_id >= RF_MAX_DEV)
    return false; // dev_id la (khung loi/gia) khong the theo doi - cu xu ly, khong dedup duoc
  bool known = dev_bitmap_get(s_dev_seq_known_bitmap, dev_id);
  if (known && s_dev_last_seq[dev_id] == seq)
    return true; // trung -> da xu ly roi
  s_dev_last_seq[dev_id] = seq;
  dev_bitmap_set(s_dev_seq_known_bitmap, dev_id, true);
  return false;
}

// ----- Bo ghep manh (reassembly) - CHI theo doi 1 nguon dang do dang tai 1
// thoi diem (du dung cho gateway RS485 ban chat noi tiep/half-duplex) -----
static uint8_t reasm_dev = 0xFF;
static uint8_t reasm_total = 0;
static uint16_t reasm_have_mask = 0; // bitmask cac manh da nhan (10 bit du cho RF_MAX_FRAG=10)
static uint8_t reasm_have_cnt = 0;
static uint8_t reasm_buf[RF_MAX_FRAG * RF_CHUNK_MAX];
static uint16_t reasm_len = 0;

static bool s_msg_ready = false;
static uint8_t s_msg_buf[RF_MAX_FRAG * RF_CHUNK_MAX];
static uint16_t s_msg_len = 0;

static void build_pipe_address(uint8_t net_id, uint8_t addr[5]) {
  // Tron network_id vao nhieu byte de tranh dia chi lap lai don dieu (khuyen
  // nghi cua Nordic la tranh cac dia chi toan 0/1 hoac lap byte giong het nhau).
  addr[0] = 0xC2;
  addr[1] = (uint8_t)(0x39 ^ net_id);
  addr[2] = 0x9E;
  addr[3] = net_id;
  addr[4] = (uint8_t)(0x7A ^ (net_id << 1));
}

static void apply_address() {
  uint8_t addr[5];
  build_pipe_address(s_netid, addr);
  radio.openWritingPipe(addr);
  radio.openReadingPipe(1, addr);
}

void rf_init(uint8_t channel, uint8_t net_id, uint8_t dev_id) {
  s_channel = channel;
  s_netid = net_id;
  s_dev_id = dev_id;

  memset(s_dev_last_seq, 0, sizeof(s_dev_last_seq));
  memset(s_dev_seq_known_bitmap, 0, sizeof(s_dev_seq_known_bitmap));
  memset(s_dev_last_seen_s, 0, sizeof(s_dev_last_seen_s));
  memset(s_dev_link_up_bitmap, 0, sizeof(s_dev_link_up_bitmap));
  reasm_dev = 0xFF;
  s_msg_ready = false;

  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS); // nhay nhat, chiu nhieu tot nhat
  radio.setCRCLength(RF24_CRC_16);
  radio.setAutoAck(false); // kenh broadcast: nhieu may cung nghe 1 dia chi
  radio.setRetries(0, 0);  // khong dung ARQ phan cung (vo nghia khi tat auto-ack)
  radio.disableDynamicPayloads();
  radio.setPayloadSize(RF_PAYLOAD_SIZE);
  radio.setChannel(s_channel);

  apply_address();
  radio.startListening();
}

bool rf_ok() {
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  bool ok = radio.isChipConnected();
  xSemaphoreGive(g_muSPI);
  return ok;
}

void rf_set_channel(uint8_t ch) {
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  s_channel = ch;
  radio.setChannel(s_channel);
  xSemaphoreGive(g_muSPI);
}
uint8_t rf_get_channel() { return s_channel; }

void rf_set_network_id(uint8_t id) {
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  s_netid = id;
  apply_address();
  xSemaphoreGive(g_muSPI);
}
uint8_t rf_get_network_id() { return s_netid; }

uint8_t rf_get_dev_id() { return s_dev_id; }

static void send_one_frame(const rf_frame_t *f) {
  radio.stopListening();
  radio.write(f, RF_PAYLOAD_SIZE);
  radio.startListening();
}

bool rf_send(const uint8_t *data, uint16_t len) {
  uint16_t max_len = (uint16_t)RF_CHUNK_MAX * RF_MAX_FRAG;
  if (len == 0 || len > max_len)
    return false;

  uint8_t frag_total = (uint8_t)((len + RF_CHUNK_MAX - 1) / RF_CHUNK_MAX);
  uint8_t seq = s_tx_seq++;

  // Khoa SPI cho CA QUA TRINH gui (co the nhieu manh x nhieu lan lap redundant
  // TX, tong cong co the mat vai trieu giay) - khong nha ra giua chung de tranh
  // flash task (flashmem.cpp) chen vao giua lam hong giao dich SPI dang do dang.
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  for (uint8_t f = 0; f < frag_total; f++) {
    rf_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    uint16_t off = (uint16_t)f * RF_CHUNK_MAX;
    uint8_t chunk_len = (uint8_t)min((uint16_t)RF_CHUNK_MAX, (uint16_t)(len - off));

    frame.dev_id = s_dev_id;
    frame.seq = seq;
    frame.frag_idx = f;
    frame.frag_total = frag_total;
    frame.len = chunk_len;
    memcpy(frame.payload, data + off, chunk_len);
    frame.crc16 = crc16_modbus((const uint8_t *)&frame, RF_HDR_LEN + chunk_len);

    // Gui lap lai s_redundant_tx lan (khong ACK) de tang do tin cay khi nhieu.
    // s_redundant_tx tu dong tang/giam theo chat luong link thuc te - xem
    // rf_heartbeat_tick() ben duoi (giong co che "adaptive rate/redundancy"
    // cua cac bo telemetry radio).
    for (uint8_t r = 0; r < s_redundant_tx; r++) {
      send_one_frame(&frame);
      s_tx_cnt++;
      delayMicroseconds(300); // khoang cach nho giua cac lan lap
    }
  }
  xSemaphoreGive(g_muSPI);
  return true;
}

// ----- HEARTBEAT: khung dieu khien nho, KHONG phai du lieu Modbus - dung
// frag_idx = 0xFF lam "sentinel" (du lieu that luon co frag_idx 0..RF_MAX_FRAG-1
// va frag_total >= 1, nen khong the trung voi gia tri nay) de ben nhan phan
// biet duoc day la heartbeat, khong dua vao bo ghep manh/hop thu ung dung. -----
static void send_heartbeat_frame() {
  rf_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.dev_id = s_dev_id;
  frame.seq = s_tx_seq++; // dung chung bo dem seq - dedup phia nhan van dung binh thuong
  frame.frag_idx = 0xFF;
  frame.frag_total = 0;
  frame.len = 0;
  frame.crc16 = crc16_modbus((const uint8_t *)&frame, RF_HDR_LEN);

  // Heartbeat gui thua (mat 1 cai khong sao, ky sau se gui lai) nen chi can
  // gui lap 2 lan la du - khong ton bang thong nhu khung du lieu that.
  // Tu khoa g_muSPI rieng (self-contained) vi ham nay duoc goi tu
  // rf_heartbeat_tick() NGOAI vung khoa cua vong lap radio.available().
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  for (uint8_t r = 0; r < 2; r++) {
    send_one_frame(&frame);
    s_tx_cnt++;
    s_hb_tx_cnt++;
    delayMicroseconds(300);
  }
  xSemaphoreGive(g_muSPI);
}

// LUU Y THU TU KHOA (tranh deadlock): ham nay co the duoc goi trong luc RF
// task DANG GIU g_muSPI (tu rf_process() -> handle_frame()), roi lai khoa
// g_muSerial de in - tuc thu tu CO DINH la "g_muSPI truoc, g_muSerial sau"
// (long nhau theo huong nay LUON AN TOAN). O BAT KY noi nao khac trong code
// (vd hal.cpp CLI), TUYET DOI KHONG duoc giu g_muSerial (dbg_lock) roi moi
// goi ham can g_muSPI (rf_*/flash_*) - phai lam nguoc lai: goi xong cac ham
// rf_*/flash_* (tu khoa/mo g_muSPI rieng, doc lap) de lay ket qua, SAU DO moi
// dbg_lock() de in - tranh 2 task cho nhau (A giu SPI cho Serial, B giu
// Serial cho SPI).
static void note_link_alive(uint8_t dev_id) {
  // ----- Trang thai TOAN MANG (giu nguyen hanh vi cu - dung cho redundant-TX
  // tu dieu chinh, khong tach theo dev_id, xem rf_link.h) -----
  bool was_up = s_link_up;
  s_last_rx_ms = millis();
  s_peer_id = dev_id;
  s_period_had_rx = true; // 1 "nhip" tot cho ky danh gia hien tai
  if (!was_up) {
    s_link_up = true;
    dbg_lock();
    SerialDBG.print("RF LINK: UP (peer=");
    SerialDBG.print(dev_id);
    SerialDBG.println(")");
    dbg_unlock();
  }

  // ----- Trang thai THEO TUNG dev_id (muc 3.3/7.1) - bao dung thiet bi nao
  // dang len/xuong, khong bi "trung binh hoa" nhu bien toan cuc o tren. Dung
  // don vi GIAY (uint16_t) thay vi ms - chi phuc vu hien thi CLI chan doan,
  // xem giai thich toi uu RAM trong rf_link.h (dau dinh nghia RF_MAX_DEV). -----
  if (dev_id < RF_MAX_DEV) {
    bool dev_was_up = dev_bitmap_get(s_dev_link_up_bitmap, dev_id);
    s_dev_last_seen_s[dev_id] = (uint16_t)(millis() / 1000);
    if (s_dev_last_seen_s[dev_id] == 0)
      s_dev_last_seen_s[dev_id] = 1; // tranh trung voi sentinel "chua tung thay" (0) o giay dau tien sau boot
    if (!dev_was_up) {
      dev_bitmap_set(s_dev_link_up_bitmap, dev_id, true);
      dbg_lock();
      SerialDBG.print("RF LINK: UP (dev_id=");
      SerialDBG.print(dev_id);
      SerialDBG.println(")");
      dbg_unlock();
    }
  }
}

// Quet toan bo dev_id da tung thay, phat hien thiet bi nao vua qua han
// RF_LINK_TIMEOUT_S ma khong nhan duoc gi -> bao LINK DOWN RIENG cho dung
// dev_id do (khac voi bien s_link_up toan cuc o rf_heartbeat_tick()). Goi moi
// chu ky heartbeat - quet 64 phan tu la re, khong dang lo ve hieu nang.
static void scan_dev_link_timeout() {
  uint16_t now_s = (uint16_t)(millis() / 1000);
  for (uint16_t i = 0; i < RF_MAX_DEV; i++) {
    uint8_t dev_id = (uint8_t)i;
    if (!dev_bitmap_get(s_dev_link_up_bitmap, dev_id))
      continue; // chi thiet bi dang UP moi can kiem tra qua han
    uint16_t age_s = (uint16_t)(now_s - s_dev_last_seen_s[dev_id]); // wrap-safe (giong kieu millis())
    if (age_s >= RF_LINK_TIMEOUT_S) {
      dev_bitmap_set(s_dev_link_up_bitmap, dev_id, false);
      dbg_lock();
      SerialDBG.print("RF LINK: DOWN (dev_id=");
      SerialDBG.print(dev_id);
      SerialDBG.print(", ");
      SerialDBG.print(age_s);
      SerialDBG.println("s khong nhan duoc gi)");
      dbg_unlock();
    }
  }
}

static void rf_heartbeat_tick() {
  uint32_t now = millis();

  // 1) Gui heartbeat dinh ky (non-blocking, khong dung delay() dai)
  if ((uint32_t)(now - s_hb_last_tx_ms) >= RF_HB_PERIOD_MS) {
    s_hb_last_tx_ms = now;
    send_heartbeat_frame();
  }

  // 2) Het thoi gian cho ma khong nhan duoc gi -> bao LINK DOWN (giong Mission
  // Planner bao "mat ket noi" khi qua han heartbeat MAVLink)
  if (s_link_up && (uint32_t)(now - s_last_rx_ms) >= RF_LINK_TIMEOUT_MS) {
    s_link_up = false;
    dbg_lock();
    SerialDBG.print("RF LINK: DOWN (peer=");
    SerialDBG.print(s_peer_id);
    SerialDBG.print(", ");
    SerialDBG.print((uint32_t)(now - s_last_rx_ms));
    SerialDBG.println("ms khong nhan duoc gi)");
    dbg_unlock();
  }

  // 2b) Cung logic tren nhung TACH RIENG theo tung dev_id (muc 3.3/7.1) - biet
  // chinh xac dung thiet bi nao dang DOWN thay vi chi 1 dong log gop ca mang.
  scan_dev_link_timeout();

  // 3) Cu moi ky RF_HB_PERIOD_MS: danh gia ky do "tot" (co nhan) hay "mat
  // trang" (khong nhan gi) -> tich luy ty le mat + tu dong tang/giam do du
  // phong (redundant TX). Day la phan "toi uu forward it mat goi" chinh:
  // link cang nhieu/xau -> tu tang so lan gui lap; link on dinh tro lai ->
  // giam ve muc binh thuong de khong chiem kenh/ton dien qua muc can thiet.
  if ((uint32_t)(now - s_period_start_ms) >= RF_HB_PERIOD_MS) {
    s_period_start_ms = now;
    if (s_period_had_rx) {
      s_period_ok_cnt++;
      s_period_miss_streak = 0;
      s_period_ok_streak++;
      if (s_period_ok_streak >= RF_ADAPT_OK_STREAK && s_redundant_tx > RF_REDUNDANT_TX_MIN) {
        s_redundant_tx--;
        s_period_ok_streak = 0;
        dbg_lock();
        SerialDBG.print("RF: link on dinh, giam do du phong con ");
        SerialDBG.println(s_redundant_tx);
        dbg_unlock();
      }
    } else {
      s_period_miss_cnt++;
      s_period_ok_streak = 0;
      s_period_miss_streak++;
      if (s_period_miss_streak >= RF_ADAPT_MISS_STREAK && s_redundant_tx < RF_REDUNDANT_TX_MAX) {
        s_redundant_tx++;
        s_period_miss_streak = 0;
        dbg_lock();
        SerialDBG.print("RF: link kem, tang do du phong len ");
        SerialDBG.println(s_redundant_tx);
        dbg_unlock();
      }
    }
    s_period_had_rx = false;
  }
}

static void reasm_reset(uint8_t dev_id, uint8_t total) {
  reasm_dev = dev_id;
  reasm_total = total;
  reasm_have_mask = 0;
  reasm_have_cnt = 0;
  reasm_len = 0;
  memset(reasm_buf, 0, sizeof(reasm_buf));
}

static void handle_frame(const rf_frame_t *f) {
  uint16_t hdr_and_payload = RF_HDR_LEN + f->len;
  uint16_t calc = crc16_modbus((const uint8_t *)f, hdr_and_payload);
  if (calc != f->crc16) {
    s_rx_crcerr_cnt++;
    return;
  }

  if (dedup_check_and_update(f->dev_id, f->seq)) {
    s_rx_dup_cnt++;
    return; // ban sao gui lap (redundant TX) - da xu ly roi
  }

  // Bat ky khung hop le nao (du lieu that hay heartbeat) deu chung to link
  // dang song -> cap nhat "lan nghe cuoi" phuc vu phat hien LINK UP/DOWN va
  // dieu chinh do du phong. Lam TRUOC khi kiem tra frag_idx/frag_total vi
  // heartbeat co gia tri sentinel (frag_idx=0xFF) khong hop le voi du lieu that.
  note_link_alive(f->dev_id);

  if (f->frag_idx == 0xFF) {
    // Khung dieu khien HEARTBEAT - khong phai du lieu Modbus, dung xong la thoi.
    s_hb_rx_cnt++;
    return;
  }

  if (f->frag_total > RF_MAX_FRAG || f->frag_idx >= f->frag_total) {
    s_rx_fragdrop_cnt++;
    return;
  }

  if (f->frag_total == 1) {
    // Ban tin 1 manh - giao thang, khong can bo ghep
    uint16_t n = f->len;
    if (n > sizeof(s_msg_buf))
      n = sizeof(s_msg_buf);
    memcpy(s_msg_buf, f->payload, n);
    s_msg_len = n;
    s_msg_ready = true;
    s_rx_ok_cnt++;
    return;
  }

  // Nhieu manh: neu khac nguon/seq dang ghep do -> bat dau lai (gateway RS485
  // ban chat noi tiep, khong can ghep nhieu ban tin song song)
  if (reasm_dev != f->dev_id || reasm_total != f->frag_total) {
    reasm_reset(f->dev_id, f->frag_total);
  }

  uint16_t off = (uint16_t)f->frag_idx * RF_CHUNK_MAX;
  if (off + f->len <= sizeof(reasm_buf)) {
    memcpy(reasm_buf + off, f->payload, f->len);
    if (f->frag_idx == f->frag_total - 1)
      reasm_len = off + f->len; // manh cuoi cung quyet dinh tong chieu dai
    if (!(reasm_have_mask & (uint16_t)(1u << f->frag_idx)))
      reasm_have_cnt++;
    reasm_have_mask |= (uint16_t)(1u << f->frag_idx);
  }

  if (reasm_have_cnt >= reasm_total) {
    uint16_t n = reasm_len;
    if (n > sizeof(s_msg_buf))
      n = sizeof(s_msg_buf);
    memcpy(s_msg_buf, reasm_buf, n);
    s_msg_len = n;
    s_msg_ready = true;
    s_rx_ok_cnt++;
    reasm_dev = 0xFF; // xong, cho ban tin ke tiep
  }
}

void rf_process() {
  // Giu g_muSPI xuyen suot vong lap nhan (co the goi handle_frame() ->
  // note_link_alive() -> dbg_lock() long ben trong - dung thu tu khoa da quy
  // dinh o tren, AN TOAN). rf_heartbeat_tick() goi SAU KHI da nha g_muSPI o
  // day vi no tu quan ly khoa rieng (SPI va Serial la 2 khoi doc lap, khong
  // long nhau trong ham do).
  xSemaphoreTake(g_muSPI, portMAX_DELAY);
  while (radio.available()) {
    rf_frame_t f;
    radio.read(&f, RF_PAYLOAD_SIZE);
    handle_frame(&f);
  }
  xSemaphoreGive(g_muSPI);
  rf_heartbeat_tick(); // gui heartbeat dinh ky + phat hien LINK UP/DOWN + tu
                       // dieu chinh do du phong - luon chay du bridge on/off
}

bool rf_available() { return s_msg_ready; }

uint16_t rf_read(uint8_t *buf, uint16_t bufsize) {
  if (!s_msg_ready)
    return 0;
  uint16_t n = s_msg_len;
  if (n > bufsize)
    n = bufsize;
  memcpy(buf, s_msg_buf, n);
  s_msg_ready = false;
  return n;
}

void rf_get_stats(uint32_t *tx, uint32_t *rx_ok, uint32_t *rx_dup,
                   uint32_t *rx_crcerr, uint32_t *rx_fragdrop) {
  if (tx) *tx = s_tx_cnt;
  if (rx_ok) *rx_ok = s_rx_ok_cnt;
  if (rx_dup) *rx_dup = s_rx_dup_cnt;
  if (rx_crcerr) *rx_crcerr = s_rx_crcerr_cnt;
  if (rx_fragdrop) *rx_fragdrop = s_rx_fragdrop_cnt;
}

void rf_reset_stats() {
  s_tx_cnt = s_rx_ok_cnt = s_rx_dup_cnt = s_rx_crcerr_cnt = s_rx_fragdrop_cnt = 0;
  s_hb_tx_cnt = s_hb_rx_cnt = 0;
  s_period_ok_cnt = s_period_miss_cnt = 0;
  // KHONG reset s_link_up/s_last_rx_ms/s_redundant_tx: day la trang thai
  // "dang song" cua link, khong phai bo dem thong ke tich luy.
}

// ----- Heartbeat / Link health TOAN MANG: getter cho CLI (hal.cpp) va cac module khac -----
bool rf_link_up() { return s_link_up; }
uint8_t rf_link_peer_id() { return s_peer_id; }
uint32_t rf_link_age_ms() { return (uint32_t)(millis() - s_last_rx_ms); }
uint8_t rf_get_redundancy() { return s_redundant_tx; }

// ----- Heartbeat / Link health THEO TUNG dev_id (muc 3.3/7.1) -----
bool rf_dev_seen(uint8_t dev_id) {
  return (dev_id < RF_MAX_DEV) ? s_dev_seen[dev_id] : false;
}
bool rf_dev_link_up(uint8_t dev_id) {
  return (dev_id < RF_MAX_DEV) ? s_dev_link_up[dev_id] : false;
}
uint32_t rf_dev_age_ms(uint8_t dev_id) {
  if (dev_id >= RF_MAX_DEV || !s_dev_seen[dev_id])
    return 0xFFFFFFFFUL; // chua tung thay
  return (uint32_t)(millis() - s_dev_last_seen_ms[dev_id]);
}

uint16_t rf_get_loss_permille() {
  uint32_t total = s_period_ok_cnt + s_period_miss_cnt;
  if (total == 0)
    return 0;
  return (uint16_t)(((uint32_t)s_period_miss_cnt * 1000UL) / total);
}

void rf_get_hb_stats(uint32_t *hb_tx, uint32_t *hb_rx) {
  if (hb_tx) *hb_tx = s_hb_tx_cnt;
  if (hb_rx) *hb_rx = s_hb_rx_cnt;
}
