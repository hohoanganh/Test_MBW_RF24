#pragma once
#include <Arduino.h>

// RTC PCF85063ATL (I2C1: PB6=SCL, PB7=SDA, dia chi 0x51), pin nuoi CR1220 (BT1).

bool rtc_get_time(uint8_t *hh, uint8_t *mm, uint8_t *ss);
bool rtc_set_time(uint8_t hh, uint8_t mm, uint8_t ss);
void rtc_print();
