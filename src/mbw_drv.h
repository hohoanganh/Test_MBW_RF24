#pragma once
#include <Arduino.h>

// Umbrella: gop tat ca driver trong src/drivers/* + drv_init() goi tung
// *_init() theo dung thu tu (giong pattern opms_drv.h / pdu_drv.h).

#include "drivers/drv_common.h"
#include "drivers/dipsw.h"
#include "drivers/rs485.h"
#include "drivers/flashmem.h"
#include "drivers/rtc.h"
#include "drivers/ledbuzz.h"
#include "drivers/rf_link.h"
#include "drivers/bridge.h"
#include "drivers/watchdog.h"

void drv_init();
