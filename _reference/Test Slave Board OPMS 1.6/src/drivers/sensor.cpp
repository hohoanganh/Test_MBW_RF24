#include "../opms_common.h"
#include "sensor.h"
#include "drv_common.h"
#include <math.h>

// =====================================================================
//  NTC10K - mach chia ap:  3V3 --- R_FIXED(10K) --- node(ADC) --- NTC --- GND
//    V_node = 3V3 * R_ntc / (R_ntc + R_FIXED)
//    => R_ntc = R_FIXED * adc / (ADC_MAX - adc)
//  Beta:  1/T = 1/T0 + (1/B) * ln(R_ntc / R25)   (T tinh bang Kelvin)
//  Khi KHONG cam cam bien, node bi keo len ~3V3 (adc ~ ADC_MAX) -> tra ve 255.
// =====================================================================
static const float NTC_R_FIXED = 10000.0f;  // tro keo len 3V3 (ohm)
static const float NTC_R25     = 10000.0f;  // dien tro NTC tai 25C (10K)
static const float NTC_BETA    = 3950.0f;   // he so Beta; doi theo datasheet cam bien
static const float NTC_T0_K    = 298.15f;   // 25C -> Kelvin
static const int   NTC_OPEN    = 255;       // gia tri bao "ho / chua cam"
static const int   NTC_OPEN_ADC = 4000;     // adc >= nguong nay -> coi nhu ho mach

// =====================================================================
//  Do am SHT30-ARP-B2.5KS - ngo ra ANALOG ratiometric (datasheet Sensirion).
//    V_RH = VDD * (0.1 + 0.8 * RH/100)        (dao dong 10%..90% cua VDD)
//    => RH(%) = (V_RH/VDD - 0.1) / 0.8 * 100  = 125 * (V_RH/VDD) - 12.5
//  Phan cung board: cam bien cap 3V3, qua op-amp buffer ×1 -> ADC (Vref 3V3).
//    V_RH = V_adc / HUM_GAIN ;  ratio = V_RH / HUM_VDD
//    (khi HUM_VDD = ADC_VREF va GAIN = 1  ->  ratio = adc / ADC_MAX)
//  *Neu doi nguon cap / mach op-amp: chinh HUM_VDD va HUM_GAIN cho dung.
//
//  XA TU LOC: dau cam co tu loc nho; khi CHUA cam tu nay co the giu dien ap
//  gia tao gia tri do am. Truoc moi lan do, keo chan ADC xuong LOW (OUTPUT) de
//  XA tu, roi tha ve INPUT_ANALOG:
//    - Co cam bien : ngo ra analog cua cam bien keo lai dien ap  -> doc %RH.
//    - Chua cam    : duong tin hieu THA NOI (khong treo len 3V3), da xa ve ~0
//                    -> tra ve HUM_NO_SENSOR (0).
// =====================================================================
static const float    HUM_VDD          = 3.3f;  // nguon cap cam bien (V) - moc ratiometric
static const float    HUM_GAIN         = 1.0f;  // he so op-amp (V_adc / V_RH); buffer = 1.0
static const float    HUM_RATIO_MIN    = 0.07f; // ratio < nguong (sau khi xa) -> chua cam
static const int      HUM_NO_SENSOR    = 0;     // gia tri bao "chua cam" (duong tin hieu ~0)
static const uint32_t HUM_DISCHARGE_MS = 3;     // thoi gian keo chan xuong de xa tu loc
static const uint32_t HUM_SETTLE_MS    = 15;    // cho cam bien (neu co) tai lap dien ap

static const uint32_t NTC_PIN[4] = {NTC1, NTC2, NTC3, NTC4};
static const uint32_t HUM_DAT[2] = {HUM1_DATA, HUM2_DATA};
static const uint32_t HUM_ENP[2] = {HUM1_EN, HUM2_EN};

// Doc 1 cong do am SHT30-ARP -> %RH (0..100). Chua cam -> 0 (xa tu, tha noi ~0).
static int hum_rh_pct(uint32_t pin) {
  // --- Xa tu loc o dau cam ---
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(HUM_DISCHARGE_MS);
  pinMode(pin, INPUT_ANALOG);
  delay(HUM_SETTLE_MS);          // cho cam bien (neu co) keo lai dien ap

  int adc = analogRead(pin);
  float v_adc = (float)adc / (float)ADC_MAX * ADC_VREF;  // dien ap tai chan ADC
  float v_rh  = v_adc / HUM_GAIN;                        // bu he so op-amp
  float ratio = v_rh / HUM_VDD;                          // ratiometric so voi VDD

  // Chua cam: da xa tu, duong tha noi quanh 0 (khong treo len 3V3).
  if (ratio < HUM_RATIO_MIN) return HUM_NO_SENSOR;

  float rh = (ratio - 0.1f) / 0.8f * 100.0f;
  if (rh < 0.0f)   rh = 0.0f;
  if (rh > 100.0f) rh = 100.0f;
  return (int)lroundf(rh);
}

// Doc 1 cong NTC -> nhiet do (C, da lam tron). Tra ve 255 neu ho/chua cam.
static int ntc_temp_c(uint32_t pin) {
  int adc = analogRead(pin);

  if (adc >= NTC_OPEN_ADC) return NTC_OPEN;   // ho mach: node keo len ~3V3
  if (adc <= 0)            return NTC_OPEN;    // chap GND / loi doc

  float r_ntc = NTC_R_FIXED * (float)adc / (float)(ADC_MAX - adc);
  float tK = 1.0f / (1.0f / NTC_T0_K + (1.0f / NTC_BETA) * logf(r_ntc / NTC_R25));
  return (int)lroundf(tK - 273.15f);
}

void sensor_init() {
  for (uint8_t i = 0; i < 4; i++) pinMode(NTC_PIN[i], INPUT_ANALOG);
  pinMode(HUM_DAT[0], INPUT_ANALOG);
  pinMode(HUM_DAT[1], INPUT_ANALOG);
  // Nguon 5V cam bien do am -> OFF (an toan)
  pinMode(HUM1_EN, OUTPUT); digitalWrite(HUM1_EN, LOW);
  pinMode(HUM2_EN, OUTPUT); digitalWrite(HUM2_EN, LOW);
}

int ntc_read(uint8_t ch) {
  if (ch < 1 || ch > 4) return -1;
  return ntc_temp_c(NTC_PIN[ch - 1]);   // nhiet do C; 255 = ho/chua cam
}

int hum_read(uint8_t ch) {
  if (ch < 1 || ch > 2) return -1;
  return hum_rh_pct(HUM_DAT[ch - 1]);   // %RH; 0 = chua cam (da xa tu loc)
}

void hum_power(uint8_t ch, bool on) {
  if (ch < 1 || ch > 2) return;
  digitalWrite(HUM_ENP[ch - 1], on ? HIGH : LOW);
}
