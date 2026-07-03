#pragma once
#include <Arduino.h>

// =====================================================================
//  OPMS 1.6 - DRIVER (umbrella header)
//  Cac driver chuc nang duoc tach theo nhom trong src/drivers/:
//    io      - GPO, GPI (74HC165), dieu khien Relay AC
//    fan     - Quat 48V + Quat nho 12V (DEV fan) + doc rpm
//    sensor  - Nhiet do NTC + Do am RHT + nguon 5V cam bien
//    acmeter - Do dong AC qua CT (EmonLib)
//    system  - Flash W25Q80 + version board (+ RTC sau nay)
//    comm    - RS485 (nguon/cong/debug) + UART Orange Pi
//  Pin map: src/opms_common.h
// =====================================================================
#include "drivers/io.h"
#include "drivers/fan.h"
#include "drivers/sensor.h"
#include "drivers/acmeter.h"
#include "drivers/system.h"
#include "drivers/comm.h"

void drv_init();   // khoi tao toan bo driver (goi cac *_init theo nhom)
