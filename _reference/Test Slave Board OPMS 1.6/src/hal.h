#pragma once
#include <Arduino.h>

// =====================================================================
//  OPMS 1.6 - HAL (Hardware Abstraction Layer)
//  Buoc 1: Console USART1, LED nhip, buzzer, CLI co ban.
// =====================================================================

void hal_init();

// ===== LED / BUZZER =====
void led_life_toggle();        // nhap LED_LIFE (heartbeat, "nap xong nhay = OK")
void led_run_set(bool on);     // LED_RUN
void buzzer_beep(uint16_t ms); // keu non-blocking trong 'ms'
void buzzer_update();          // goi trong loop() de tat buzzer dung gio

// ===== Console =====
void uart_log(const char *msg);

// ===== CLI (xu ly lenh tu Console) =====
void cli_process();
void cli_prompt();   // in dau nhac "OPMS> "
