#include "ledbuzz.h"
#include "../mbw_common.h"
#include "../rtos_glue.h"

static bool s_muted = false;
static uint32_t s_buzzer_off_at = 0;

void ledbuzz_init() {
  pinMode(LED_LIFE, OUTPUT);
  digitalWrite(LED_LIFE, LOW);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, BUZZER_OFF_LEVEL);

  pinMode(USER_BTN, INPUT_PULLUP);
}

void led_life_set(bool on) { digitalWrite(LED_LIFE, on ? HIGH : LOW); }
void led_life_toggle() { digitalWrite(LED_LIFE, !digitalRead(LED_LIFE)); }

void buzzer_set_mute(bool m) { s_muted = m; }
bool buzzer_is_muted() { return s_muted; }

void buzzer_beep(uint16_t time_ms) {
  if (s_muted)
    return;
  digitalWrite(BUZZER, BUZZER_ON_LEVEL);
  s_buzzer_off_at = millis() + time_ms;
  if (s_buzzer_off_at == 0)
    s_buzzer_off_at = 1; // tranh trung gia tri "tat" (0)
}

void buzzer_update() {
  if (s_buzzer_off_at && (int32_t)(millis() - s_buzzer_off_at) >= 0) {
    digitalWrite(BUZZER, BUZZER_OFF_LEVEL);
    s_buzzer_off_at = 0;
  }
}

void btn_process() {
  static bool pressed = false;
  static uint32_t t_edge = 0;
  const uint32_t DEBOUNCE_MS = 50;

  bool down = (digitalRead(USER_BTN) == LOW);
  uint32_t now = millis();

  if (down != pressed && (now - t_edge) >= DEBOUNCE_MS) {
    pressed = down;
    t_edge = now;
    dbg_lock();
    SerialDBG.println(pressed ? "BTN: DOWN" : "BTN: UP");
    dbg_unlock();
  }
}

bool btn_is_pressed() { return digitalRead(USER_BTN) == LOW; }
