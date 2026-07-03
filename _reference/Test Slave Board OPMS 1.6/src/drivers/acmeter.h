#pragma once
#include <Arduino.h>

// =====================================================================
//  AC current meter driver - do dong AC qua bien dong (CT) bang EmonLib.
//  Thu vien: openenergymonitor/EmonLib (calcIrms).
//  Chan do: ADC_CT. He so calib ICAL CAN CHUAN tren ban test.
// =====================================================================
void acmeter_init();
int  ac_current_mA();    // dong AC RMS (mA)
