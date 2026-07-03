#include "../opms_common.h"
#include "acmeter.h"
#include <EmonLib.h>

// =====================================================================
//  EmonLib: do dong AC qua bien dong (CT) tren chan ADC_CT.
//  Phan cung OPMS 1.6:
//    - CT  : ZMCT103U, ti so 1000:1 (vong = 1000)
//    - R_burden: 100 ohm
//  ICAL = ti so CT / R_burden = 1000 / 100 = 10.0
//    (V_burden = I_so_cap * R = (I_pri/1000)*100 ; ICAL = I_pri/V_burden = 1000/100)
//  Bias (DC offset): modul CT co chan ghi 1.65V = DUNG GIUA dai 3.3V
//    (= ADC_COUNTS/2 ~ 2048). EmonLib tu bam offset bang bo loc DC, khoi tao
//    o ADC_COUNTS/2 nen hoi tu ngay -> KHONG can chinh offset thu cong.
//  Luu y:
//    - Da dat ADC_BITS=12 (build_flags) -> ADC_COUNTS = 4096, dung 12-bit STM32.
//    - EmonLib (non-AVR) gia dinh Vcc = 3300 mV (khop ADC_VREF 3.3V).
//    - Tinh chinh nho neu lech: do bang dong ho kep dong roi chinh ICAL theo
//      ty le (ICAL_moi = ICAL * I_dong_ho / I_app).
// =====================================================================
static EnergyMonitor s_emon;
static const double AC_ICAL = 10.0;     // ZMCT103U 1000:1 + burden 100 ohm

void acmeter_init() {
  pinMode(ADC_CT, INPUT_ANALOG);
  s_emon.current(ADC_CT, AC_ICAL);      // gan chan do + he so calib
  s_emon.calcIrms(1480);                // doc bo de bo loc DC bam ve bias 1.65V
}

int ac_current_mA() {
  s_emon.calcIrms(1480);                // lam am: on dinh bo loc DC (bias 1.65V)
  double irms = s_emon.calcIrms(1480);  // ~ vai chu ky 50Hz -> Irms (A)
  return (int)(irms * 1000.0 + 0.5);    // A -> mA
}
