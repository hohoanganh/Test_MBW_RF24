#include "../opms_common.h"
#include "fan.h"

// ----- Quat 48V (RS1751B48H): FG open-collector, dong co 4 cuc -----
// RPM = F * 60 / FAN_PULSES_PER_REV  (F = tan so FG, Hz); 2 xung/vong (4 cuc).
static const int FAN_PULSES_PER_REV = 2;

static const uint32_t FAN_PIN[4]  = {FAN1_PWM, FAN2_PWM, FAN3_PWM, FAN4_PWM};
static const uint32_t FANT_PIN[4] = {FAN1_TACH, FAN2_TACH, FAN3_TACH, FAN4_TACH};
static const uint32_t DFAN_CTR[3] = {DEV_FAN1_CTR, DEV_FAN2_CTR, DEV_FAN3_CTR};
static const uint32_t DFAN_FB[3]  = {DEV_FAN1_FB, DEV_FAN2_FB, DEV_FAN3_FB};

void fan_init() {
  // Fan PWM -> OUTPUT, OFF (NPN dao muc: HIGH = TAT) ; Fan TACH -> INPUT_PULLUP
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(FAN_PIN[i], OUTPUT); digitalWrite(FAN_PIN[i], HIGH);
    pinMode(FANT_PIN[i], INPUT_PULLUP);
  }
  // DEV fan ctrl OUTPUT OFF ; feedback INPUT_PULLUP
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(DFAN_CTR[i], OUTPUT); digitalWrite(DFAN_CTR[i], LOW);
    pinMode(DFAN_FB[i], INPUT_PULLUP);
  }
}

// Doc tin hieu FG -> RPM (dung chung quat 48V & quat 12V).
static int fg_to_rpm(uint32_t pin) {
  unsigned long h = pulseIn(pin, HIGH, 60000);   // us (timeout 60ms)
  unsigned long l = pulseIn(pin, LOW, 60000);
  if (h == 0 || l == 0) return 0;                // khong co xung -> dung
  float F = 1000000.0f / (float)(h + l);         // tan so FG (Hz)
  return (int)(F * 60.0f / FAN_PULSES_PER_REV + 0.5f);
}

// ---- Quat 48V ----
// Chan dieu khien qua NPN -> DAO MUC: LOW = quat CHAY, HIGH = TAT.
void fan_set_pwm(uint8_t ch, uint8_t pct) {
  if (ch < 1 || ch > 4) return;
  if (pct > 100) pct = 100;
  uint16_t duty = (uint16_t)pct * 255 / 100;
  analogWrite(FAN_PIN[ch - 1], 255 - duty);   // dao muc (NPN)
}

void fan_set(uint8_t ch, bool on) {
  fan_set_pwm(ch, on ? 100 : 0);
}

int fan_rps(uint8_t ch) {            // ten giu nguyen; gia tri = RPM
  if (ch < 1 || ch > 4) return 0;
  return fg_to_rpm(FANT_PIN[ch - 1]);
}

// ---- Quat nho / Quat 12V (DEV fan) ----
void dev_fan_set(uint8_t ch, bool on) {
  if (ch < 1 || ch > 3) return;
  digitalWrite(DFAN_CTR[ch - 1], on ? HIGH : LOW);
}

bool dev_fan_fb(uint8_t ch) {
  if (ch < 1 || ch > 3) return false;
  return digitalRead(DFAN_FB[ch - 1]) ? true : false;
}

int dev_fan_rps(uint8_t ch) {
  if (ch < 1 || ch > 2) return 0;
  return fg_to_rpm(DFAN_FB[ch - 1]);
}
