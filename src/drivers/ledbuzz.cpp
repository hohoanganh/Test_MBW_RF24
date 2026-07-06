#include "ledbuzz.h"
#include "../mbw_common.h"
#include "../rtos_glue.h"
#include "rf_link.h"   // toggle repeater bang nut S2 (giu 3s) - muc 6 tai lieu dinh huong
#include "flashmem.h"  // repeater_save() - luu vai tro qua cac lan mat nguon

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

// GIU NUT S2 3 GIAY -> toggle che do REPEATER 1-hop (muc 6 tai lieu dinh
// huong, phuong an 2 "khong can laptop"): ky thuat vien bat/tat repeater ngay
// tai hien truong. Xac nhan: LED_LIFE nhay nhanh 3 lan + 1 tieng bip DAI khi
// BAT, 1 tieng NGAN khi TAT. Vai tro luu Flash (repeater_save) - giu qua cac
// lan mat nguon. Chay trong task CLI (uu tien thap nhat): 3 lan nhay LED
// blocking ~360ms khong anh huong RS485/RF, van diem danh watchdog kip (3s).
#define BTN_HOLD_REPEATER_MS 3000

static void repeater_toggle_by_button() {
  bool en = !rf_is_repeater();
  rf_set_repeater(en);
  repeater_save(en); // tu khoa/mo g_muSPI rieng - goi TRUOC dbg_lock() (thu tu khoa!)

  dbg_lock();
  SerialDBG.println(en ? "REPEATER: ON (nut S2, da luu Flash)"
                       : "REPEATER: OFF (nut S2, da luu Flash)");
  dbg_unlock();

  // LED nhay nhanh 3 lan + bip: dai (600ms) = BAT, ngan (150ms) = TAT
  buzzer_beep(en ? 600 : 150);
  for (int i = 0; i < 3; i++) {
    led_life_set(true);
    delay(60);
    led_life_set(false);
    delay(60);
  }
}

void btn_process() {
  static bool pressed = false;
  static uint32_t t_edge = 0;
  static bool hold_fired = false; // da toggle trong lan giu nay chua (chi 1 lan/1 lan giu)
  const uint32_t DEBOUNCE_MS = 50;

  bool down = (digitalRead(USER_BTN) == LOW);
  uint32_t now = millis();

  if (down != pressed && (now - t_edge) >= DEBOUNCE_MS) {
    pressed = down;
    t_edge = now;
    if (!pressed)
      hold_fired = false; // nha nut -> lan giu ke tiep duoc phep toggle lai
    dbg_lock();
    SerialDBG.println(pressed ? "BTN: DOWN" : "BTN: UP");
    dbg_unlock();
  }

  // Dang giu du lau -> toggle repeater DUNG 1 LAN (giu tiep khong lap lai)
  if (pressed && !hold_fired && (now - t_edge) >= BTN_HOLD_REPEATER_MS) {
    hold_fired = true;
    repeater_toggle_by_button();
  }
}

bool btn_is_pressed() { return digitalRead(USER_BTN) == LOW; }
