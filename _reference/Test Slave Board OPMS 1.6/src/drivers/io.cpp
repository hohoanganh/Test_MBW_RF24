#include "../opms_common.h"
#include "io.h"
#include "drv_common.h"

// ----- ACS712-05B (do dong ngo ra GPO): 185 mV/A -----
static const float ACS712_V_PER_A = 0.185f;   // TODO calib neu co mach khuech dai

static const uint32_t GPO_PIN[4] = {GPO1, GPO2, GPO3, GPO4};

void io_init() {
  // GPO -> OUTPUT, OFF
  for (uint8_t i = 0; i < 4; i++) { pinMode(GPO_PIN[i], OUTPUT); digitalWrite(GPO_PIN[i], LOW); }
  // Relay AC
  pinMode(ACC_CTR, OUTPUT); digitalWrite(ACC_CTR, LOW);
  // GPI 74HC165
  pinMode(GPI_LOAD, OUTPUT); digitalWrite(GPI_LOAD, HIGH);
  pinMode(GPI_EN, OUTPUT);   digitalWrite(GPI_EN, HIGH);   // CLK_INH = HIGH (chua doc)
  pinMode(GPI_CLK, OUTPUT);  digitalWrite(GPI_CLK, LOW);
  pinMode(GPI_DATA, INPUT);
  // ADC do dong ngo ra
  pinMode(GPO_CUR, INPUT_ANALOG);
  pinMode(GPO_OFFSET, INPUT_ANALOG);
}

// ---- GPO ----
void gpo_set(uint8_t ch, bool on) {
  if (ch < 1 || ch > 4) return;
  digitalWrite(GPO_PIN[ch - 1], on ? HIGH : LOW);
}

int gpo_current_mA() {
  int cur = analogRead(GPO_CUR);
  int off = analogRead(GPO_OFFSET);
  float dV = (cur - off) * (ADC_VREF / ADC_MAX);   // V
  float mA = (dV / ACS712_V_PER_A) * 1000.0f;
  if (mA < 0) mA = -mA;
  return (int)(mA + 0.5f);
}

// ---- GPI 74HC165 ----
uint8_t gpi_read_all() {
  // Chot du lieu song song
  digitalWrite(GPI_LOAD, LOW);
  delayMicroseconds(5);
  digitalWrite(GPI_LOAD, HIGH);
  digitalWrite(GPI_EN, LOW);                         // cho phep clock

  uint8_t val = 0;
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t bit = digitalRead(GPI_DATA) ? 1 : 0;
    val |= (bit << (7 - i));                         // bit dau (QH) la MSB
    digitalWrite(GPI_CLK, HIGH);
    delayMicroseconds(2);
    digitalWrite(GPI_CLK, LOW);
  }
  digitalWrite(GPI_EN, HIGH);
  return val;   // TODO: doi chieu thu tu bit GPI1..8 voi phan cung thuc te
}

// ---- Relay AC (dieu khien) ----
void ac_relay_set(bool on) {
  digitalWrite(ACC_CTR, on ? HIGH : LOW);
}
