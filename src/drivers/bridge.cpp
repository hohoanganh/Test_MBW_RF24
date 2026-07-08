#include "bridge.h"
#include "rs485.h"
#include "rf_link.h"
#include "../mbw_common.h"
#include "../rtos_glue.h"
#include <string.h>

#define MODBUS_MAX_LEN 250 // gioi han theo RF_MAX_FRAG*RF_CHUNK_MAX (rf_link.h)
// (2026-07-07: preview khi log forward dung BRIDGE_LOG_PREVIEW_MAX, dinh nghia
// cung hang doi log ngay ben duoi - xem log_forward()/bridge_log_process())

static bool s_enabled = true; // MAC DINH BAT: hanh vi chuan cua thiet bi
// 2026-07-06: log forward MAC DINH TAT cho lap dat that (Modbus master poll
// lien tuc 16 slave se in "FWD ..." day dac console + ton CPU in chuoi trong
// task uu tien cao). App test/kY thuat vien bat lai bang "bridge log on" khi
// can quan sat (app da co san nut toggle).
static bool s_log = false;
static uint8_t s_buf[MODBUS_MAX_LEN];
static uint16_t s_len = 0;
static uint32_t s_last_byte_us = 0;

// 2026-07-03: frame lam viec cua TUNG task chuyen tu stack sang STATIC.
// Truoc day moi rtos_frame_t (252 byte) nam tren stack cua
// bridge_rs485_step()/bridge_rf_step() lam TRAN STACK task RF (LED nhay 2
// xung - hook trong main.cpp) voi stack 176 word. Moi frame duoc dung TUAN TU
// trong 1 task duy nhat (s_frame_rs485 chi trong task RS485, s_frame_rf chi
// trong task RF) nen static la an toan, khong can mutex.
static rtos_frame_t s_frame_rs485; // CHI task RS485 dung (flush + nhan queue)
static rtos_frame_t s_frame_rf;    // CHI task RF dung (nhan queue + doc RF + echo debug)

static uint32_t s_cnt_rs485_to_rf = 0;
static uint32_t s_cnt_rf_to_rs485 = 0;
// Dem so khung bi ROT vi hang doi lien-task day (g_qToRF/g_qToRS485 sau = 1) -
// xem "rtos stat"/"bridge stat" (hal.cpp) de theo doi tren phan cung that.
static uint32_t s_cnt_rs485_to_rf_drop = 0;
static uint32_t s_cnt_rf_to_rs485_drop = 0;

// ===== 2026-07-08: redundant-TX TU DIEU CHINH theo LOI THUC DO - CHI TREN HUB
// (xem docs/Bao_cao_mat_frame_..., muc 11.3 huong #2) =====
// Chi so dung: "hieu so" |RS485->RF - RF->RS485| so voi so lon hon trong 2 -
// CHINH LA chi so da chung minh phan anh dung ty le request-khong-co-hoi-am
// (mat goi RF thuc su), KHAC voi heartbeat loss%o (de bi danh lua - xem case
// dev_id 1: loss%o=0 nhung van rot frame du lieu that).
//
// TAI SAO CHI CHAY TREN HUB (dev_id==0): voi Hub, MOI lenh RS485->RF la 1
// request gui cho DUNG 1 slave, ky vong DUNG 1 response RF->RS485 tra ve ->
// hieu so = mat goi that. Nhung tren 1 SLAVE, day la mang broadcast (moi
// slave deu nhan HET moi request cua toan mang, khong chi rieng dia chi cua
// no) nen RF->RS485 (request nhan duoc) LUON LON HON NHIEU RS485->RF (chi
// request DUNG dia chi thiet bi cua no moi co response That) - "hieu so" tren
// slave gan nhu luon cao NHUNG KHONG PHAI mat goi, chi la "khong phai request
// cho toi". Neu chay thuat toan nay tren slave se ratchet redundancy len MAX
// vo ich cho MOI slave, con gay nghen kenh nguoc lai (dung antipattern voi
// thuat toan auto-redund CU da bi bo - xem ghi chu 2026-07-06 trong rf_link.h,
// nhung theo huong nguoc: do "luon thay xau" thay vi "luon thay tot").
//
// PHAM VI: chi tu dieu chinh do du phong CHIEU Hub->Slave (redundancy GUI DI
// cua chinh Hub). Chieu Slave->Hub (response) VAN dung gia tri co dinh nguoi
// dung tu set rieng qua "rf redund" tren tung slave - thuat toan nay KHONG
// giai quyet duoc phan mat goi o chieu do (han che da biet, ghi trong bao cao).
//
// 2026-07-08 (vá RAM tràn 8 byte): ban dau dung 3 bien uint32_t (last_ms +
// prev_tx + prev_rx = 12 byte) -> build that bao tran 8 byte. Doi sang KICH
// HOAT THEO SO MAU (dem tu prev_tx, deu la uint16_t) THAY VI theo dong ho
// millis(): chi con 2 bien uint16_t (4 byte, giam dung 8 byte can). Tac dung
// phu: tu dieu chinh nhanh hon khi traffic cao (kiem tra ngay khi du mau,
// khong phai cho du 5s), cham hon khi traffic thap (cho du mau moi kiem tra) -
// chap nhan duoc vi day la vong dieu khien cham (+-1 moi lan, kep MIN/MAX).
// uint16_t truncate + tru uint16 van an toan voi wraparound (giong logic
// uint32 truoc day) vi delta thuc te giua 2 lan kiem tra khong bao gio gan
// toi 65536 (luon duoi nguong MIN_SAMPLE truoc khi kiem tra lai).
#define ADAPT_REDUND_MIN_SAMPLE 20  // it hon thi chua du tin cay, cho tich luy them (giong nguong app dung)
#define ADAPT_REDUND_HI_PERMILLE 80 // hieu so > 8% -> tang du phong 1 muc
#define ADAPT_REDUND_LO_PERMILLE 20 // hieu so < 2% -> giam du phong 1 muc (khong duoi DEFAULT)
static uint16_t s_adapt_prev_tx = 0, s_adapt_prev_rx = 0; // snapshot 16-bit (xem ghi chu tren)

// Goi tu bridge_rf_step() (task RF) moi vong lap - tu kiem tra "du mau chua"
// (xem ghi chu tren). Chi buoc +-1 moi lan (khong nhay thang len MAX/xuong
// MIN) de thay doi tu tu, tranh dao qua nhanh theo 1 mau nhieu don le.
static void adapt_redundancy_check() {
  if (rf_get_dev_id() != 0)
    return; // chi Hub moi tinh chi so nay dung nghia - xem giai thich o tren

  uint16_t tx16 = (uint16_t)s_cnt_rs485_to_rf;
  uint16_t d_tx = (uint16_t)(tx16 - s_adapt_prev_tx); // tru uint16 an toan voi wraparound
  if (d_tx < ADAPT_REDUND_MIN_SAMPLE)
    return; // chua du mau (VD bridge dang OFF/traffic thap) - cho tich luy them, KHONG cap nhat snapshot

  uint16_t rx16 = (uint16_t)s_cnt_rf_to_rs485;
  uint16_t d_rx = (uint16_t)(rx16 - s_adapt_prev_rx);
  s_adapt_prev_tx = tx16;
  s_adapt_prev_rx = rx16;

  // Tren Hub, d_tx (so request GUI DI) la mau so dung nghia (xem giai thich
  // tren) - KHONG can so sanh max(d_tx,d_rx) nhu ban dau. d_rx > d_tx (nhieu
  // response hon request) khong nen xay ra (da dedup (dev_id,seq) o rf_link)
  // nhung phong het truong hop hiem, coi la 0% loi thay vi tinh am.
  uint16_t diff = d_tx > d_rx ? (uint16_t)(d_tx - d_rx) : 0;
  uint16_t err_permille = (uint16_t)(((uint32_t)diff * 1000UL) / d_tx);
  uint8_t cur = rf_get_redundancy();

  if (err_permille > ADAPT_REDUND_HI_PERMILLE && cur < RF_REDUNDANT_TX_MAX) {
    rf_set_redundancy(cur + 1);
    dbg_lock();
    SerialDBG.print("BRIDGE: adaptive redund TANG len ");
    SerialDBG.print(cur + 1);
    SerialDBG.print(" (loi ky nay=");
    SerialDBG.print(err_permille / 10);
    SerialDBG.println("%)");
    dbg_unlock();
  } else if (err_permille < ADAPT_REDUND_LO_PERMILLE && cur > RF_REDUNDANT_TX_DEFAULT) {
    rf_set_redundancy(cur - 1);
    dbg_lock();
    SerialDBG.print("BRIDGE: adaptive redund GIAM ve ");
    SerialDBG.print(cur - 1);
    SerialDBG.println(" (dieu kien da on dinh)");
    dbg_unlock();
  }
}

void bridge_init() {
  s_len = 0;
  s_enabled = true; // forward RS485<->Wireless la hanh vi mac dinh cua san pham
  SerialDBG.println("BRIDGE: ON (mac dinh, forward RS485 <-> Wireless)");
}

void bridge_set_enable(bool en) {
  s_enabled = en;
  s_len = 0; // xoa buffer do dang khi doi mode, tranh ghep 2 khung khac nhau
  SerialDBG.println(en ? "BRIDGE: ON" : "BRIDGE: OFF");
}
bool bridge_is_enabled() { return s_enabled; }

void bridge_set_log(bool en) {
  s_log = en;
  SerialDBG.println(en ? "BRIDGE LOG: ON" : "BRIDGE LOG: OFF");
}
bool bridge_log_enabled() { return s_log; }

#define BRIDGE_LOG_DIR_RS485_TO_RF 0
#define BRIDGE_LOG_DIR_RF_TO_RS485 1

// ===== 2026-07-07 (fix mat frame): hang doi LOG "FWD ..." tach khoi task RS485 =====
// TRUOC DAY: ham log_forward() in THANG ra SerialDBG (dbg_lock + vai print/
// println) NGAY TRONG task RS485 (uu tien cao nhat). Do do la thao tac CHAM
// (vai ms tuy do dai dong + baud debug UART), no da BLOCK CHINH task can giu
// dung khoang lang 3.5 ky tu (>=1.75ms) de tach khung Modbus - lam mat/ghep sai
// khung RS485 KE TIEP moi khi bat "bridge log on" (xem docs/Bao_cao_mat_frame_...).
//
// FIX (lan 1): doi sang 1 hang doi FreeRTOS (xQueueCreateStatic) - task RS485
// chi enqueue khong-block, task CLI (uu tien THAP NHAT) moi in that su.
// FIX (lan 2, 2026-07-07): ban build that BAO "region RAM overflowed by 88
// bytes" - StaticQueue_t cua FreeRTOS ton ~70-80 byte "control block" MOI
// queue (list/con tro noi bo), qua dat tren chip chi 10KB RAM chi de doi 1
// dong debug. DOI SANG RING BUFFER TU VIET TAY duoi day (khong dung FreeRTOS
// queue): task RS485 la PRODUCER DUY NHAT (chi ghi s_log_head), task CLI la
// CONSUMER DUY NHAT (chi ghi s_log_tail) - moi ben chi doc bien 8-bit cua ben
// kia (doc/ghi 1 bien 8-bit tren Cortex-M la nguyen tu/an toan, khong can
// mutex), tiet kiem duoc ~70-80 byte control block so voi dung xQueue.
// 2026-07-07 (lan 3): build that tiep tuc bao "RAM overflowed by 8 bytes" sau
// khi them 3 ham rs485_send_start/poll/pending (bridge fix lan 2, ~9-12 byte
// bien tinh moi trong rs485.cpp). LUC DO giam preview 8->4 byte de tiet kiem
// RAM - NHUNG lam BUNG NO false-positive "(>16B)" o app giam sat: app dung dau
// "..." (xuat hien khi preview bi cat ngan) lam dau hieu khung bi ghep/qua
// dai; khung Modbus binh thuong da dai 7-8 byte nen VOI PREVIEW CHI 4 BYTE,
// GAN NHU MOI khung hop le (du dai dung chuan, KHONG he ghep) deu bi cat va
// gan nham nhan "(>16B)" - xem docs/Bao_cao_mat_frame_..., muc 8. Do dai frame
// THAT trong log van dung 7-8 byte, CRC lỗi=0 - giao tiep khong he hong, chi
// la bug hien thi/phan loai do preview qua ngan.
//
// 2026-07-07 (lan 4): FIX dut diem - doi hang doi tu "2 cho x 4 byte preview"
// sang "1 cho x 8 byte preview" (dung 1 co bool don gian thay vi head/tail,
// vi voi 1 cho duy nhat khong the dung kieu so sanh head/tail cu - se luon bi
// coi la "day"). Vua khoi phuc preview du 8 byte (het bug hien thi tren), vua
// TON IT RAM HON ban 2 cho x 4 byte (12+1=13 byte so voi 8*2+2=18 byte).
#define BRIDGE_LOG_PREVIEW_MAX 8 // khoi phuc ve 8 - du hien het 1 khung Modbus ngan gon, khong con bi "..." cat ngan gay hieu nham

typedef struct {
  uint8_t dir; // 0 = RS485->RF, 1 = RF->RS485
  uint16_t len;
  uint8_t preview[BRIDGE_LOG_PREVIEW_MAX];
} bridge_log_entry_t;

// Hang doi 1-cho (KHONG phai ring buffer head/tail): task RS485 (producer)
// CHI dat s_log_valid tu false->true, task CLI (consumer) CHI dat tu true->
// false - moi ben chi ghi 1 chieu duy nhat nen an toan tren Cortex-M ma khong
// can mutex (giong tinh than head/tail truoc day, chi don gian hoa cho 1 cho).
static bridge_log_entry_t s_log_slot;
static volatile bool s_log_valid = false;

// Non-blocking, goi tu task RS485 (producer). Tra ve false (bo qua, KHONG
// cho task doi/block) neu cho dang co 1 dong CHUA KIP in - chap nhan duoc vi
// day chi la dong debug hien thi, khong phai duong du lieu that.
static bool log_queue_push(const bridge_log_entry_t &e) {
  if (s_log_valid)
    return false; // dang co 1 dong cho task CLI in, chua kip lay ra - bo qua dong nay
  s_log_slot = e;
  s_log_valid = true; // ghi SAU CUNG (sau khi du lieu da copy xong)
  return true;
}

// Non-blocking, goi tu task CLI (consumer). Tra ve false neu khong co gi cho.
static bool log_queue_pop(bridge_log_entry_t &e) {
  if (!s_log_valid)
    return false;
  e = s_log_slot;
  s_log_valid = false;
  return true;
}

// CHI ENQUEUE (KHONG BAO GIO block) - xem giai thich o tren. Viec IN THAT SU
// duoc doi sang bridge_log_process(), goi tu task CLI (uu tien THAP NHAT) -
// xem ham do cuoi file.
static void log_forward(uint8_t dir, const uint8_t *buf, uint16_t len) {
  if (!s_log)
    return;
  bridge_log_entry_t e;
  e.dir = dir;
  e.len = len;
  uint16_t n = len < BRIDGE_LOG_PREVIEW_MAX ? len : BRIDGE_LOG_PREVIEW_MAX;
  memcpy(e.preview, buf, n);
  log_queue_push(e);
}

// Khoang lang gap khung ~3.5 ky tu (chuan Modbus RTU), tinh theo baud hien tai
// cua RS485 (10 bit/ky tu: start+8data+stop, khong parity).
static uint32_t frame_gap_us() {
  uint32_t baud = rs485_get_baud();
  if (baud == 0)
    baud = 9600;
  uint32_t char_us = (uint32_t)(10000000UL / baud); // us cho 1 ky tu
  uint32_t gap = (char_us * 35) / 10;                // 3.5 ky tu
  if (gap < 1750)
    gap = 1750; // toi thieu 1.75ms cho baud thap theo khuyen nghi Modbus
  return gap;
}

// Day 1 khung hoan chinh vua gom tu RS485 vao g_qToRF cho TASK RF gui that -
// KHONG goi rf_send() truc tiep tai day nua (rf_send co the mat vai trieu
// giay lap redundant TX, se lam task RS485 tre qua han phat hien khoang lang
// 3.5 ky tu cua khung Modbus KE TIEP). Non-blocking (timeout=0): hang doi day
// (dang xu ly khung truoc) -> ROT khung nay, dem vao s_cnt_rs485_to_rf_drop.
static void flush_rs485_to_rf() {
  if (s_len == 0)
    return;
  rtos_frame_t &f = s_frame_rs485; // static, xem ghi chu dau file
  f.len = s_len;
  memcpy(f.data, s_buf, s_len);
  if (xQueueSend(g_qToRF, &f, 0) == pdPASS) {
    s_cnt_rs485_to_rf++;
    log_forward(BRIDGE_LOG_DIR_RS485_TO_RF, s_buf, s_len);
  } else {
    s_cnt_rs485_to_rf_drop++;
  }
  s_len = 0;
}

// ----- Goi tu TASK RS485 (uu tien cao nhat) -----
// 2026-07-07 (fix mat frame, lan 2): TRUOC DAY chieu RF->RS485 goi rs485_send()
// (BLOCKING - cho flush() truyen xong THAT SU, co the toi vai ms) NGAY TRONG
// ham nay - trong luc do, chieu RS485->RF (o tren) KHONG duoc chay lai, nen
// khoang lang 3.5 ky tu tach khung Modbus bi "dong bang" du khung tiep theo
// dang toi -> mat/ghep byte dau (xem docs/Bao_cao_mat_frame_..., muc 6 - da do
// duoc tan suat ghep khung KHONG doi truoc/sau khi sua log o lan 1, chung to
// day moi la nguyen nhan chinh). FIX: dung rs485_send_start()/rs485_send_poll()
// (non-blocking, xem rs485.h) - ham nay LUON tra ve nhanh, khong bao gio block.
//
// 2026-07-08 (fix mat frame, lan 5): do qua dem (12.4 gio) cho thay tan suat
// ghep khung VAN KHONG DOI (~41.6 lan/gio) DU DA co fix lan 2 - chung to fix
// do CHUA du. Ly do: fix lan 2 loai bo duoc viec BLOCK CPU cua flush(), nhung
// van "BO QUA HOAN TOAN" viec gom byte RS485->RF trong SUOT thoi gian dang tu
// TX (guard !rs485_send_pending() o duoi) - tao ra 1 "cua so mu" dung y het
// thoi luong nhu truoc (chi khac co che: chu dong bo qua thay vi bi block).
// Neu LUC BAT DAU tu TX da co san 1 phan lenh dang gom do (s_len>0, chua du
// khoang lang de flush), phan do bi "dong bang" xuyen suot cua so mu nay, roi
// GHEP LIEN voi lenh THAT SU dau tien sau khi TX xong - dung mau loi quan sat
// duoc. FIX: khi rs485_send_poll() BAO VUA XONG 1 lan TX, XA BO (khong xu ly)
// bat ky phan dang gom do dang truoc do (s_len=0 - da khong the nao hoan
// chinh duoc nua vi ta vua chiem bus) VA xa sach moi byte "echo"/rac co the
// da lot vao RX trong luc TX (mot so module RS485 echo lai chinh du lieu no
// vua phat len RO khi DE dang bat), dat lai s_last_byte_us cho SACH - dam bao
// lenh dau tien duoc gom SAU thoi diem nay luon la 1 khung MOI, khong dinh voi
// bat ky gi truoc do.
void bridge_rs485_step() {
  // Hoan tat TX dang cho tu vong truoc (neu co) - goi DAU TIEN, KHONG block.
  if (rs485_send_poll()) {
    // VUA xong 1 lan TX - xem giai thich "fix lan 5" o tren.
    while (rs485_read() >= 0) {
      // xa sach byte echo/rac tich luy trong luc TX, KHONG dua vao s_buf
    }
    s_len = 0;
    s_last_byte_us = micros();
  }

  // ----- Chieu RS485 -> RF: gom byte, phat hien khoang lang gap khung -----
  // BO QUA hoan toan khi dang tu minh TX (rs485_send_pending()): vat ly khong
  // the co gi that su den tu master trong luc ta dang giu (drive) bus nua-song-
  // cong, va tranh doc nham byte "echo" cua chinh minh vao s_buf. "Cua so mu"
  // nay VAN CON (khong the tranh hoan toan neu khong doc RX luc dang TX), NHUNG
  // gio da duoc XU LY DUNG CACH ngay khi TX xong (xem tren) nen khong con gay
  // ghep khung nua - chi mat toi da 1 request neu master gui trung luc bridge
  // dang tu TX (hiem, va Modbus master da co timeout/retry rieng).
  if (s_enabled && !rs485_send_pending()) {
    int c;
    while ((c = rs485_read()) >= 0) {
      if (s_len < MODBUS_MAX_LEN)
        s_buf[s_len++] = (uint8_t)c;
      s_last_byte_us = micros();
    }
    if (s_len > 0 && (uint32_t)(micros() - s_last_byte_us) >= frame_gap_us()) {
      flush_rs485_to_rf();
    }
  }

  // ----- Chieu RF -> RS485: lay khung da nhan tu task RF (neu co), BAT DAU
  // gui non-blocking (chi khi khong co TX nao dang do dang - hang doi sau=1
  // nen luon co the doi 1 vong sau). -----
  if (!rs485_send_pending()) {
    rtos_frame_t &f = s_frame_rs485; // static, xem ghi chu dau file
    if (xQueueReceive(g_qToRS485, &f, 0) == pdPASS) {
      if (s_enabled && f.len > 0) {
        rs485_send_start(f.data, f.len);
        s_cnt_rf_to_rs485++;
        log_forward(BRIDGE_LOG_DIR_RF_TO_RS485, f.data, f.len);
      }
      // Neu bridge dang OFF: khung van bi lay ra khoi hang doi (tranh ket dong
      // g_qToRS485 mai o trang thai day) nhung khong day ra RS485 that.
    }
  }
}

// ----- Goi tu TASK RF (uu tien trung binh) -----
void bridge_rf_step() {
  rf_process(); // nhan khung RF + heartbeat - luon chay du bridge on/off
  adapt_redundancy_check(); // 2026-07-08: tu dieu chinh redund theo loi thuc do (chi Hub - xem giai thich o tren)

  if (s_enabled) {
    // Lay khung RS485->RF (neu co) tu task RS485, gui that qua RF (co the mat
    // vai trieu giay do redundant TX - khong sao vi task nay uu tien thap hon
    // RS485, khong lam tre viec tach khung Modbus).
    rtos_frame_t &f = s_frame_rf; // static, xem ghi chu dau file
    if (xQueueReceive(g_qToRF, &f, 0) == pdPASS) {
      rf_send(f.data, f.len);
    }

    // Chieu RF -> RS485: day khung nhan duoc vao g_qToRS485 cho task RS485.
    // Dung lai s_frame_rf (da xong viec voi khung gui o tren - tuan tu).
    if (rf_available()) {
      rtos_frame_t &out = s_frame_rf;
      uint16_t n = rf_read(out.data, sizeof(out.data));
      if (n > 0) {
        out.len = n;
        if (xQueueSend(g_qToRS485, &out, 0) != pdPASS)
          s_cnt_rf_to_rs485_drop++;
      }
    }
  } else if (rf_available()) {
    // Bridge OFF: giu lai hanh vi debug cu (khong forward, chi echo ra console
    // de ky thuat vien xem thu RF co nhan duoc gi khong khi dang test rieng
    // le cac lenh CLI "rf tx <text>"/"rs485 <text>").
    // Dung lai s_frame_rf.data thay vi them 250B tren stack (xem ghi chu dau file)
    uint8_t *buf = s_frame_rf.data;
    uint16_t n = rf_read(buf, sizeof(s_frame_rf.data) - 1);
    buf[n] = 0;
    dbg_lock();
    SerialDBG.print("RF RX: ");
    SerialDBG.println((const char *)buf);
    dbg_unlock();
  }
}

void bridge_get_stats(uint32_t *rs485_to_rf, uint32_t *rf_to_rs485) {
  if (rs485_to_rf) *rs485_to_rf = s_cnt_rs485_to_rf;
  if (rf_to_rs485) *rf_to_rs485 = s_cnt_rf_to_rs485;
}
void bridge_get_drop_stats(uint32_t *rs485_to_rf_drop, uint32_t *rf_to_rs485_drop) {
  if (rs485_to_rf_drop) *rs485_to_rf_drop = s_cnt_rs485_to_rf_drop;
  if (rf_to_rs485_drop) *rf_to_rs485_drop = s_cnt_rf_to_rs485_drop;
}
void bridge_reset_stats() {
  s_cnt_rs485_to_rf = s_cnt_rf_to_rs485 = 0;
  s_cnt_rs485_to_rf_drop = s_cnt_rf_to_rs485_drop = 0;
}

// ----- Goi tu TASK CLI (uu tien THAP NHAT), moi vong lap -----
// Rut cac dong "FWD ..." da duoc log_forward() day vao ring buffer (KHONG
// block) tu task RS485 va IN THAT SU ra console tai day (dbg_lock/unlock nhu
// truoc). Lam o task thap uu tien nhat de KHONG BAO GIO anh huong toi khoang
// lang 3.5 ky tu tach khung Modbus cua task RS485 - xem giai thich day du o
// dau file (muc "hang doi LOG") va log_forward() o tren.
// "budget": rut TOI DA vai dong/lan goi de 1 lan goi khong qua lau (task CLI
// con phai lam viec khac trong cung vong lap: DIP/nut/coi - xem main.cpp).
void bridge_log_process() {
  bridge_log_entry_t e;
  uint8_t budget = 4;
  while (budget-- && log_queue_pop(e)) {
    dbg_lock();
    SerialDBG.print("FWD ");
    SerialDBG.print(e.dir == BRIDGE_LOG_DIR_RS485_TO_RF ? "RS485->RF" : "RF->RS485");
    SerialDBG.print(": ");
    SerialDBG.print(e.len);
    SerialDBG.print(" bytes:");
    uint16_t n = e.len < BRIDGE_LOG_PREVIEW_MAX ? e.len : BRIDGE_LOG_PREVIEW_MAX;
    for (uint16_t i = 0; i < n; i++) {
      SerialDBG.print(' ');
      if (e.preview[i] < 0x10)
        SerialDBG.print('0');
      SerialDBG.print(e.preview[i], HEX);
    }
    if (e.len > n)
      SerialDBG.print(" ...");
    SerialDBG.println();
    dbg_unlock();
  }
}
