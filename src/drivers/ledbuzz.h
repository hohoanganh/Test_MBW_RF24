#pragma once
#include <Arduino.h>

// LED_LIFE (PB8, qua Q4), Buzzer (PB0, qua Q1 - COI PASSIVE kich xung PWM bang
// tone(), giong AK_MCU) va nut nhan nguoi dung S2 (PA11, INPUT_PULLUP).

void ledbuzz_init();

void led_life_set(bool on);
void led_life_toggle();

// Kich coi PASSIVE bang tone() (PWM tan so freq) trong time_ms - non-blocking,
// tone() tu tat sau time_ms bang timer (giong buzzer_beep cua AK_MCU_Test_3I0).
void buzzer_beep(uint16_t freq, uint16_t time_ms);
void buzzer_update();  // giu de tuong thich (tone() tu tat - ham nay khong lam gi)
void buzzer_set_mute(bool m);
bool buzzer_is_muted();

void btn_process(); // poll nut S2, in "BTN: DOWN/UP" khi doi trang thai
bool btn_is_pressed();
