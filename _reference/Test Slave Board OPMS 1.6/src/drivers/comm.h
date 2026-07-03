#pragma once
#include <Arduino.h>

// =====================================================================
//  Comm driver: RS485 (nguon 12V + 4 cong + cong cu debug) va UART Orange Pi.
// =====================================================================
void comm_init();

// ---- Nguon 12V RS485 ----
void rs485_power(bool on);               // RS485_PWREN
bool rs485_power_good();                 // RS485_PG

// ---- Cong RS485 ----
int  rs485_probe(uint8_t port, const char *msg);  // port = 1..4, tra ve so byte nhan
void rs485_burst(uint8_t port, const char *msg, int count);  // gui lap de soi TX
// Che do NEN (KHONG block Console; task Comm xu ly). port 0/sai = TAT.
void rs485_monitor(uint8_t port);   // bat monitor nen (nhan & in RX)
void rs485_txloop(uint8_t port);    // bat txloop nen (giu DIR HIGH + phat 200ms)
void rs485_bg_stop();               // tat moi che do nen
int  rs485_bg_mode();               // 0=off, 1=monitor, 2=txloop
void rs485_bg_poll();               // task Comm goi dinh ky (da giu mutex UART)

// ---- Giao tiep Orange Pi (USART3) - scaffold ----
bool pi_selftest();              // loopback UART Orange Pi (chap TX-RX) -> true neu OK
int  pi_read_avail(uint8_t *buf, int maxn);   // doc non-blocking byte tu Orange Pi (task Comm)
void pi_monitor_set(bool on);    // bat/tat in "PI_RX>" o nen (dinh nghia trong main.cpp)
bool pi_monitor_get();           // trang thai monitor Orange Pi
int  rs485_read_avail(uint8_t *buf, int maxn); // doc non-blocking byte RS485 (ca 2 nhom)
