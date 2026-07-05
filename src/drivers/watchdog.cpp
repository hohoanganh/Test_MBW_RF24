#include "watchdog.h"
#include <IWatchdog.h> // thu vien bundled san trong STM32duino core (stm32duino/Arduino_Core_STM32), khong can them lib_deps

static bool s_boot_was_reset = false;

void wdt_init(uint32_t timeout_us) {
  // Doc + xoa co RCC IWDGRSTF NGAY LUC NAY (truoc begin()), cache lai - de
  // wdt_boot_was_reset() (vd goi tu CLI "wdt stat") doc lai bao nhieu lan
  // cung duoc ma khong lam mat/thay doi gia tri.
  s_boot_was_reset = IWatchdog.isReset(true);
  IWatchdog.begin(timeout_us); // tu clamp trong khoang [IWDG_MIN_TIMEOUT, getMaxTimeout()] cua chip neu vuot nguong
}

void wdt_feed() {
  IWatchdog.reload();
}

bool wdt_boot_was_reset() {
  return s_boot_was_reset;
}
