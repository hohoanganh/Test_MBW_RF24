#include "rtc.h"
#include "../mbw_common.h"
#include "../rtos_glue.h"
#include <string.h>
#include <stdio.h>

// RTC dung I2C (Wire) - bus rieng, KHONG dung chung voi SPI (g_muSPI), va chi
// duoc goi tu 1 task duy nhat (CLI, qua lenh "rtc"/"rtc set") nen khong can
// mutex rieng cho Wire trong kien truc toi gian nay. Van dung dbg_lock() cho
// dong in de dong bo voi console dung chung giua 3 task.

static uint8_t bcd2dec(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t dec2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

bool rtc_get_time(uint8_t *hh, uint8_t *mm, uint8_t *ss) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x04); // reg Seconds
  if (Wire.endTransmission() != 0)
    return false;

  if (Wire.requestFrom(RTC_ADDR, 3) != 3)
    return false;

  *ss = bcd2dec(Wire.read() & 0x7F);
  *mm = bcd2dec(Wire.read() & 0x7F);
  *hh = bcd2dec(Wire.read() & 0x3F);
  return true;
}

bool rtc_set_time(uint8_t hh, uint8_t mm, uint8_t ss) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x04);
  Wire.write(dec2bcd(ss));
  Wire.write(dec2bcd(mm));
  Wire.write(dec2bcd(hh));
  return Wire.endTransmission() == 0;
}

void rtc_print() {
  uint8_t hh, mm, ss;
  char buf[16];

  if (rtc_get_time(&hh, &mm, &ss))
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hh, mm, ss);
  else
    strcpy(buf, "--:--:--");

  dbg_lock();
  SerialDBG.print("RTC: ");
  SerialDBG.println(buf);
  dbg_unlock();
}
