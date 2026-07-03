#pragma once
#include <Arduino.h>

// =====================================================================
//  Sensor driver: Nhiet do NTC (4 cong) + Do am RHT (2 cong) + nguon 5V.
// =====================================================================
void sensor_init();

int  ntc_read(uint8_t ch);               // ch = 1..4, tra ve nhiet do (C); 255 = khong cam
int  hum_read(uint8_t ch);               // ch = 1..2, tra ve %RH (SHT30-ARP); 0 = khong cam (da xa tu)
void hum_power(uint8_t ch, bool on);     // ch = 1..2 (nguon 5V cam bien)
