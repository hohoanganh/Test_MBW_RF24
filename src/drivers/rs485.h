#pragma once
#include <Arduino.h>

// RS485 (USART2, THVD1420, DIR = PA1). Dung cho ca 2 muc dich:
//  - Test CLI (rs485 <text> / rsl loopback / baud rs485 <n>)
//  - Bridge that (bridge.cpp doc/ghi qua rs485_available()/rs485_read()/rs485_send())

void rs485_init();

uint32_t rs485_get_baud();
void rs485_set_baud(uint32_t b);

void rs485_send(const uint8_t *data, uint16_t len); // BLOCKING - chi dung cho CLI/loopback (khong nam trong duong tach khung Modbus)
void rs485_send_str(const char *msg);

// ----- 2026-07-07 (fix mat frame, lan 2): API non-blocking cho bridge.cpp -----
// rs485_send() (o tren) BLOCK cho toi khi flush() xong THAT SU (10 bit/byte /
// baud - vi du 8 byte o 9600 baud ~8.3ms), VUOT XA nguong 1.75ms tach khung
// Modbus. Goi rs485_send() (blocking) tu task RS485 (bridge_rs485_step(), chieu
// RF->RS485) se "dong bang" viec doc/tach khung Modbus chieu RS485->RF trong
// suot thoi gian do - gay mat/ghep byte dau khung ke tiep (xem docs/Bao_cao_mat_
// frame_..., muc 6). 3 ham duoi day thay the rs485_send() TRONG bridge.cpp:
// bat dau gui (write khong cho flush), roi "poll" (khong block) o MOI vong lap
// task - uoc luong thoi gian truyen theo baud (tuong tu frame_gap_us() trong
// bridge.cpp) thay vi cho flush() that su bao xong.
void rs485_send_start(const uint8_t *data, uint16_t len); // bat dau TX, KHONG block
bool rs485_send_poll();      // goi moi vong lap (non-blocking); tra ve true DUNG 1 LAN khi vua xong (DIR da ve LOW)
bool rs485_send_pending();   // true neu dang co TX chua xong (KHONG doc/gop khung RS485->RF luc nay)

int rs485_available();
int rs485_read(); // -1 neu khong co byte

void rs485_process(); // "rs485mon": khi bat, forward RX ra console de debug
void rs485_set_monitor(bool en);
bool rs485_monitor_enabled();

bool rs485_loopback(); // test noi bo: can noi tat A-B (hoac RJ11 loop) ben ngoai
