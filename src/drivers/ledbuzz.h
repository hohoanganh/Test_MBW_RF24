#pragma once
#include <Arduino.h>

// LED_LIFE (PB8, qua Q4), Buzzer (PB0, qua Q1, active-high) va nut nhan
// nguoi dung S2 (PA11, INPUT_PULLUP - KHONG phai NRST cung).

void ledbuzz_init();

void led_life_set(bool on);
void led_life_toggle();

void buzzer_beep(uint16_t time_ms); // non-blocking, tat dung gio qua buzzer_update()
void buzzer_update();               // goi trong loop()
void buzzer_set_mute(bool m);
bool buzzer_is_muted();

void btn_process(); // poll nut S2, in "BTN: DOWN/UP" khi doi trang thai
bool btn_is_pressed();
