#pragma once
#include <Arduino.h>

// RTC PCF85063ATL (I2C1: PB6=SCL, PB7=SDA, dia chi 0x51), pin nuoi CR1220 (BT1).

bool rtc_get_time(uint8_t *hh, uint8_t *mm, uint8_t *ss);
bool rtc_set_time(uint8_t hh, uint8_t mm, uint8_t ss);
// Ngay/thang/nam (nam 2 chu so 00-99) - thanh ghi Days(0x07)/Months(0x09)/Years(0x0A)
bool rtc_get_date(uint8_t *dd, uint8_t *mo, uint8_t *yy);
bool rtc_set_date(uint8_t dd, uint8_t mo, uint8_t yy);
void rtc_print();
