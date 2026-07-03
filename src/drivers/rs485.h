#pragma once
#include <Arduino.h>

// RS485 (USART2, THVD1420, DIR = PA1). Dung cho ca 2 muc dich:
//  - Test CLI (rs485 <text> / rsl loopback / baud rs485 <n>)
//  - Bridge that (bridge.cpp doc/ghi qua rs485_available()/rs485_read()/rs485_send())

void rs485_init();

uint32_t rs485_get_baud();
void rs485_set_baud(uint32_t b);

void rs485_send(const uint8_t *data, uint16_t len);
void rs485_send_str(const char *msg);

int rs485_available();
int rs485_read(); // -1 neu khong co byte

void rs485_process(); // "rs485mon": khi bat, forward RX ra console de debug
void rs485_set_monitor(bool en);
bool rs485_monitor_enabled();

bool rs485_loopback(); // test noi bo: can noi tat A-B (hoac RJ11 loop) ben ngoai
