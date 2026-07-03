#pragma once
#include <Arduino.h>

// =====================================================================
//  IO driver: GPO (ngo ra MOSFET + do dong), GPI (74HC165), dieu khien Relay AC.
//  (Do dong AC tach rieng -> drivers/acmeter.* dung EmonLib.)
// =====================================================================
void io_init();

// ---- GPO (4 ngo ra) + do dong tong (ACS712) ----
void gpo_set(uint8_t ch, bool on);       // ch = 1..4
int  gpo_current_mA();                    // dong tong ngo ra (mA)

// ---- GPI 8 dau vao (74HC165) ----
uint8_t gpi_read_all();                   // 8 bit, bit0 = GPI1 ... bit7 = GPI8

// ---- Relay AC (chi dieu khien dong/ngat) ----
void ac_relay_set(bool on);
