#include "opms_common.h"
#include "opms_drv.h"

// =====================================================================
//  OPMS 1.6 - DRIVER core: khoi tao toan bo driver theo nhom.
//  Phan trien khai chi tiet nam trong src/drivers/*.cpp.
// =====================================================================
void drv_init() {
  analogReadResolution(12);   // ADC 12-bit (dung chung cho io / sensor / acmeter)
  io_init();
  fan_init();
  sensor_init();
  acmeter_init();
  system_init();
  comm_init();
}
