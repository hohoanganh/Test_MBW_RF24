#include "mbw_drv.h"
#include "mbw_common.h"
#include "hal.h"

void drv_init() {
  dip_init();
  rs485_init();
  flash_init();
  ledbuzz_init();
  bridge_init();

  // RF: kenh + Network ID mac dinh lay tu DIP switch luc khoi dong. Neu can
  // doi Network ID luc dang chay (khong reset board) dung lenh CLI "rf netid".
  uint8_t netid = dip_network_id();
  rf_init(120, netid); // 120 = kenh mac dinh (2.520GHz vung, tranh WiFi 1-11)

  // RS485 baud lay theo DIP SW7-8 luc khoi dong
  rs485_set_baud(dip_baud_value());

  SerialDBG.print("RF: ");
  SerialDBG.println(rf_ok() ? "nRF24L01 OK" : "nRF24L01 NOT FOUND");
  SerialDBG.print("NETID (from DIP): ");
  SerialDBG.println(netid);
}
