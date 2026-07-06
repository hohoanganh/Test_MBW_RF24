#include "mbw_drv.h"
#include "mbw_common.h"
#include "hal.h"

void drv_init() {
  dip_init();
  rs485_init();
  flash_init();
  ledbuzz_init();
  bridge_init();

  // 2026-07-04 (xem docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md muc 3.2):
  // DEV_ID (dinh danh RF rieng cho TUNG BOARD, doc lap voi dia chi Modbus)
  // doc tu DIP SW1-6 cho MOI board NHU NHAU - KHONG co nhanh code rieng cho
  // Hub: dev_id == 0 tu nhien la quy uoc Hub (DIP de nguyen, khong gat bit
  // nao), 1-63 la Slave.
  uint8_t dev_id = dip_dev_id();

  // NET_ID (hang so CHUNG cho ca 1 deployment: hub + moi slave cung 1 gia
  // tri) khong con doc tu DIP nua - doc tu Flash, cau hinh 1 lan qua CLI
  // "net id <0-63>" (luu qua cac lan mat nguon). Neu CHUA tung cau hinh (flash
  // trong = NET_ID_UNSET), canh bao ro tren console va tam dung NET_ID=0 de
  // board van khoi dong duoc (ky thuat vien BAT BUOC phai set lai qua CLI
  // truoc khi dua vao mang that).
  uint8_t netid = net_id_load();
  bool netid_configured = (netid != NET_ID_UNSET);
  if (!netid_configured)
    netid = 0;

  rf_init(120, netid, dev_id); // 120 = kenh mac dinh (2.520GHz vung, tranh WiFi 1-11)

  // Co REPEATER 1-hop (muc 6 tai lieu dinh huong) doc tu Flash - board da
  // duoc chi dinh lam repeater (qua CLI "rf repeater on" hoac giu nut S2 3s)
  // giu nguyen vai tro sau moi lan mat nguon, khong can cau hinh lai.
  bool repeater = repeater_load();
  rf_set_repeater(repeater);

  // RS485 baud lay theo DIP SW7-8 luc khoi dong
  rs485_set_baud(dip_baud_value());

  SerialDBG.print("RF: ");
  SerialDBG.println(rf_ok() ? "nRF24L01 OK" : "nRF24L01 NOT FOUND");
  SerialDBG.print("DEV_ID (DIP): ");
  SerialDBG.print(dev_id);
  SerialDBG.println(dev_id == 0 ? " (HUB)" : " (SLAVE)");
  if (netid_configured) {
    SerialDBG.print("NET_ID (Flash): ");
    SerialDBG.println(netid);
  } else {
    SerialDBG.println("CANH BAO: NET_ID CHUA CAU HINH (dang tam dung 0) - "
                       "dung CLI \"net id <0-63>\" de set truoc khi lap dat that!");
  }
  if (repeater)
    SerialDBG.println("REPEATER: ON (Flash) - board nay dang PHAT LAI khung cua "
                       "node khac (1-hop). Tat bang \"rf repeater off\" hoac giu nut S2 3s.");
}
