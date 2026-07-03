#pragma once
#include <Arduino.h>

// =====================================================================
//  Fan driver: Quat 48V (4 kenh, PWM + TACH) va Quat nho 12V (DEV fan).
// =====================================================================
void fan_init();

// ---- Quat 48V ----
void fan_set(uint8_t ch, bool on);          // ch = 1..4 (bat = 100%)
void fan_set_pwm(uint8_t ch, uint8_t pct);  // ch = 1..4, pct 0..100 (% toc do)
int  fan_rps(uint8_t ch);                    // toc do (rpm tu TACH), 0 neu dung

// ---- Quat nho / Quat 12V (DEV fan) ----
void dev_fan_set(uint8_t ch, bool on);       // ch = 1..3
bool dev_fan_fb(uint8_t ch);                 // chan feedback (1 = co tin hieu)
int  dev_fan_rps(uint8_t ch);                // ch = 1..2, toc do (rpm tu FB)
