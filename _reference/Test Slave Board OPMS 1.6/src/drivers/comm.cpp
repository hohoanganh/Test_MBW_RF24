#include "../opms_common.h"
#include "comm.h"

// Console debug serial (khai bao o main.cpp) - dung de echo RS485 RX / PI RX
extern HardwareSerial SerialDBG;

// RS485: USART2 (nhom A, cong 1-2) va UART4 (nhom B, cong 3-4)
static HardwareSerial SerialRS485A(RS485A_RX, RS485A_TX);  // PA3 / PA2
static HardwareSerial SerialRS485B(RS485B_RX, RS485B_TX);  // PC11 / PC10
// UART <-> Orange Pi (USART3, PB11/PB10)
static HardwareSerial SerialPI(PI_UART_RX, PI_UART_TX);

// ===== Che do NEN RS485 (KHONG block Console; xu ly boi task Comm RTOS) =====
//  s_bg_mode: 0=off, 1=monitor (nhan & in RX), 2=txloop (giu DIR HIGH + phat 200ms)
static volatile int s_bg_mode = 0;
static volatile int s_bg_port = 0;

static HardwareSerial *bg_ser(int port) {
  if (port >= 1 && port <= 2) return &SerialRS485A;
  if (port >= 3 && port <= 4) return &SerialRS485B;
  return nullptr;
}
static uint32_t bg_dir(int port) {
  return (port >= 1 && port <= 2) ? RS485_DIR1 : RS485_DIR2;
}

void rs485_bg_stop() {
  s_bg_mode = 0; s_bg_port = 0;
  digitalWrite(RS485_DIR1, LOW);
  digitalWrite(RS485_DIR2, LOW);          // ca 2 nhom ve che do nhan
}

int rs485_bg_mode() { return s_bg_mode; }

// Goi dinh ky tu task Comm (da giu mutex UART). KHONG block.
void rs485_bg_poll() {
  HardwareSerial *s = bg_ser(s_bg_port);
  if (!s) return;
  if (s_bg_mode == 1) {                    // MONITOR: in moi byte nhan duoc
    while (s->available()) SerialDBG.write((char)s->read());
  } else if (s_bg_mode == 2) {             // TXLOOP: phat moi 200ms
    static uint32_t last = 0;
    if (millis() - last >= 200) { last = millis(); s->print("OPMS-RS485-TEST\r\n"); }
  }
}

void comm_init() {
  // Nguon 12V RS485: RS485_PWREN qua NPN -> HIGH = TAT (an toan luc khoi dong)
  pinMode(RS485_PWREN, OUTPUT); digitalWrite(RS485_PWREN, HIGH);
  // Huong truyen + power-good
  pinMode(RS485_DIR1, OUTPUT); digitalWrite(RS485_DIR1, LOW);  // mac dinh nhan
  pinMode(RS485_DIR2, OUTPUT); digitalWrite(RS485_DIR2, LOW);
  pinMode(RS485_PG, INPUT_PULLUP);
  SerialRS485A.begin(RS485_BAUD);
  SerialRS485B.begin(RS485_BAUD);
  SerialPI.begin(PI_UART_BAUD);
}

// ---- Nguon 12V RS485 ----
void rs485_power(bool on) {
  // Chan dieu khien qua NPN -> DAO MUC: LOW = BAT nguon, HIGH = TAT.
  digitalWrite(RS485_PWREN, on ? LOW : HIGH);
}

bool rs485_power_good() {
  return digitalRead(RS485_PG) ? true : false;
}

// ---- Cong RS485: gui tren nhom cua 'port', nghe lai tren CA HAI nhom ----
// DIR (DE/RE): HIGH = truyen (DE), LOW = nhan (RE).
int rs485_probe(uint8_t port, const char *msg) {
  HardwareSerial *s_tx, *s_rx;
  uint32_t dir_tx, dir_rx;
  if (port >= 1 && port <= 2) {
    s_tx = &SerialRS485A; dir_tx = RS485_DIR1;   // gui nhom A
    s_rx = &SerialRS485B; dir_rx = RS485_DIR2;   // nghe them nhom B
  } else if (port >= 3 && port <= 4) {
    s_tx = &SerialRS485B; dir_tx = RS485_DIR2;   // gui nhom B
    s_rx = &SerialRS485A; dir_rx = RS485_DIR1;   // nghe them nhom A
  } else return -1;

  rs485_bg_stop();                        // dung che do nen truoc khi probe
  pinMode(dir_tx, OUTPUT);
  pinMode(dir_rx, OUTPUT);
  digitalWrite(dir_tx, LOW);
  digitalWrite(dir_rx, LOW);
  delayMicroseconds(50);
  while (s_tx->available()) s_tx->read();
  while (s_rx->available()) s_rx->read();

  digitalWrite(dir_tx, HIGH);            // bat driver (DE = transmit)
  delay(1);
  s_tx->print(msg);
  s_tx->flush();
  delay(2 + (uint32_t)(20000UL / RS485_BAUD));
  digitalWrite(dir_tx, LOW);             // tra ve nhan

  char rx[64];
  int n = 0;
  uint32_t t    = millis();
  uint32_t tend = millis();
  // Nghe toi 600ms (kit co the dap tre do dang ve LCD); thoat som khi da nhan
  // duoc data va im lang > 30ms (het 1 frame phan hoi).
  while (millis() - t < 600) {
    bool got = false;
    while (s_rx->available()) { int c = s_rx->read(); if (n < 63) rx[n] = (char)c; n++; got = true; }
    while (s_tx->available()) { int c = s_tx->read(); if (n < 63) rx[n] = (char)c; n++; got = true; }
    if (got) tend = millis();
    if (n > 0 && millis() - tend > 30) break;   // da nhan xong frame -> thoat
  }
  rx[(n < 63) ? n : 63] = 0;
  SerialDBG.print("RS485RX=\""); SerialDBG.print(rx); SerialDBG.println("\"");
  return n;   // so byte nhan duoc (>0 = co phan hoi)
}

// MONITOR (nen): bat che do nhan lien tuc, task Comm se in RX ra console.
// KHONG block Console. port 0 hoac sai = TAT.
void rs485_monitor(uint8_t port) {
  if (port < 1 || port > 4) { rs485_bg_stop(); return; }
  HardwareSerial *s = bg_ser(port);
  pinMode(bg_dir(port), OUTPUT);
  digitalWrite(bg_dir(port), LOW);        // che do NHAN
  while (s->available()) s->read();
  s_bg_port = port; s_bg_mode = 1;
  SerialDBG.print("RS485 MONITOR (nen) cong "); SerialDBG.print(port);
  SerialDBG.println(" - go 'rs485stop' de dung.");
}

// Gui LAP chuoi 'msg' nhieu lan (de soi tin hieu phat).
void rs485_burst(uint8_t port, const char *msg, int count) {
  HardwareSerial *s;
  uint32_t dir;
  if (port >= 1 && port <= 2) { s = &SerialRS485A; dir = RS485_DIR1; }
  else if (port >= 3 && port <= 4) { s = &SerialRS485B; dir = RS485_DIR2; }
  else return;
  rs485_bg_stop();
  pinMode(dir, OUTPUT);
  for (int i = 0; i < count; i++) {
    digitalWrite(dir, HIGH);              // DE = transmit
    delay(1);
    s->print(msg);
    s->flush();
    delay(2 + (uint32_t)(20000UL / RS485_BAUD));
    digitalWrite(dir, LOW);               // tra ve nhan
    delay(100);
  }
  SerialDBG.print("RS485 da gui "); SerialDBG.print(count);
  SerialDBG.print(" lan tren cong "); SerialDBG.println(port);
}

// TXLOOP (nen): giu DIR HIGH + task Comm phat chuoi moi 200ms.
// KHONG block Console. port 0 hoac sai = TAT.
void rs485_txloop(uint8_t port) {
  if (port < 1 || port > 4) { rs485_bg_stop(); return; }
  pinMode(bg_dir(port), OUTPUT);
  digitalWrite(bg_dir(port), HIGH);        // giu che do truyen (DE)
  s_bg_port = port; s_bg_mode = 2;
  SerialDBG.print("RS485 TXLOOP (nen) cong "); SerialDBG.print(port);
  SerialDBG.println(" - DIR HIGH, phat moi 200ms. Go 'rs485stop' de dung.");
}

// ---- Giao tiep Orange Pi (USART3) - SCAFFOLD (loopback chap TX-RX) ----
// TODO: chot cach test that voi Orange Pi (loopback jumper / Pi echo / protocol).
bool pi_selftest() {
  const char *msg = "OPMS-PI-LOOPBACK\r\n";
  while (SerialPI.available()) SerialPI.read();   // xoa buffer
  SerialPI.print(msg);
  SerialPI.flush();
  char rx[48]; int n = 0;
  uint32_t t = millis();
  while (millis() - t < 200) {
    while (SerialPI.available()) { int c = SerialPI.read(); if (n < 47) rx[n] = (char)c; n++; }
  }
  rx[(n < 47) ? n : 47] = 0;
  SerialDBG.print("PIRX=\""); SerialDBG.print(rx); SerialDBG.println("\"");
  return (strstr(rx, "OPMS-PI-LOOPBACK") != nullptr);
}

// ---- Doc non-blocking (cho task Comm cua ban RTOS) ----
int pi_read_avail(uint8_t *buf, int maxn) {
  int n = 0;
  while (SerialPI.available() && n < maxn) buf[n++] = (uint8_t)SerialPI.read();
  return n;
}

int rs485_read_avail(uint8_t *buf, int maxn) {
  int n = 0;
  while (SerialRS485A.available() && n < maxn) buf[n++] = (uint8_t)SerialRS485A.read();
  while (SerialRS485B.available() && n < maxn) buf[n++] = (uint8_t)SerialRS485B.read();
  return n;
}
