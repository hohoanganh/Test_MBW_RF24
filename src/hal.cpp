#include "hal.h"
#include "mbw_common.h"
#include "mbw_drv.h"
#include "rtos_glue.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ===== QUY TAC KHOA g_muSerial trong file nay (tranh deadlock voi g_muSPI -
// xem giai thich day du trong rf_link.cpp) =====
// task_cli chay cli_execute() KHONG duoc giu g_muSerial (dbg_lock) trong luc
// goi cac ham rf_*/flash_* (chung tu khoa/mo g_muSPI rieng ben trong). Vi vay
// MOI NHANH lenh trong cli_execute() phai: (1) goi xong cac ham driver truoc,
// luu ket qua vao bien cuc bo, (2) SAU DO moi dbg_lock() -> in toan bo phan
// hoi -> dbg_unlock(). KHONG dbg_lock() roi moi goi rf_send()/flash_print_id()/...
// o giua.

#define CLI_BUF_SIZE 96

static char cmd_buf[CLI_BUF_SIZE];
static uint8_t cmd_idx = 0;

void hal_init() {
  SerialDBG.begin(DBG_BAUD);

  pinMode(SPI_MOSI, OUTPUT); // dam bao SPI1 duoc cau hinh truoc SPI.begin()
  SPI.begin();
  Wire.begin();
}

void uart_log(const char *msg) { SerialDBG.println(msg); }

// =========================================================
// CLI
// =========================================================
static void print_help() {
  dbg_lock();
  SerialDBG.println("id / ver / help");
  SerialDBG.println("led  (toggle LED_LIFE) / beep on|off");
  SerialDBG.println("dip  (doc DIP: DEVID + BAUD)");
  SerialDBG.println("rs485 <text>  (gui thu RS485)");
  SerialDBG.println("rsl  (loopback RS485, can noi tat A-B)");
  SerialDBG.println("rs485mon on|off  (forward RS485 RX ra console)");
  SerialDBG.println("baud rs485 <1200-921600>");
  SerialDBG.println("flash / fwr  (SPI flash W25Q128)");
  SerialDBG.println("rtc  / rtc set <hh:mm:ss>");
  SerialDBG.println("net id <0-63>  / net id  (NET_ID chung ca deployment, luu Flash)");
  SerialDBG.println("rf id  (kiem tra nRF24L01 co mat)");
  SerialDBG.println("rf ch <0-125>  / rf netid <0-63> (doi NET_ID tam thoi, KHONG luu Flash - dung 'net id' de luu)");
  SerialDBG.println("rf tx <text>  (gui khong day, dung ghep cap 2 board)");
  SerialDBG.println("rf stat / rf reset  (LINK UP/DOWN, LOSS% toan mang, REDUND, REPEATER/RELAY)");
  SerialDBG.println("rf devices  (liet ke dev_id + UP/DOWN + LOSS%o rieng tung thiet bi)");
  SerialDBG.println("rf dev <0-63>  (chi tiet link 1 dev_id rieng)");
  SerialDBG.println("rf redund <2-6>  / rf redund  (so lan gui lap CO DINH, chi RAM)");
  SerialDBG.println("rf repeater on|off  / rf repeater  (relay 1-hop cho node xa, luu Flash; hoac giu S2 3s)");
  SerialDBG.println("bridge on|off  (mac dinh ON - chuc nang cau RS485<->Wireless that)");
  SerialDBG.println("bridge log on|off  (in dong 'FWD ...' khi relay, mac dinh OFF - bat khi test)");
  SerialDBG.println("bridge stat / bridge reset");
  SerialDBG.println("rtos stat  (RAM/stack con lai cua 3 task - kiem tra RTOS)");
  SerialDBG.println("wdt stat  (watchdog IWDG: co reset lan truoc, dang feed hay khong, tuoi diem danh tung task)");
  dbg_unlock();
}

static void cli_execute(char *cmd) {

  // ----- ID / VERSION -----
  if (strcmp(cmd, "id") == 0) {
    dbg_lock();
    SerialDBG.println("ID: " DEVICE_ID " FW " FW_VERSION);
    dbg_unlock();
  }

  else if (strcmp(cmd, "ver") == 0) {
    dbg_lock();
    SerialDBG.println("FW " FW_VERSION " (" __DATE__ " " __TIME__ ")");
    dbg_unlock();
  }

  else if (strcmp(cmd, "help") == 0) {
    print_help(); // tu khoa/mo rieng
  }

  // ----- LED / BUZZER -----
  else if (strcmp(cmd, "led") == 0) {
    led_life_toggle();
    dbg_lock();
    SerialDBG.println("OK");
    dbg_unlock();
  }

  else if (strncmp(cmd, "beep", 4) == 0) {
    char st[8];
    bool have_arg = sscanf(cmd, "beep %7s", st) == 1;
    bool valid = have_arg && (strcmp(st, "on") == 0 || strcmp(st, "off") == 0);
    if (valid)
      buzzer_set_mute(strcmp(st, "off") == 0);
    dbg_lock();
    if (valid || strcmp(cmd, "beep") == 0) {
      SerialDBG.println(buzzer_is_muted() ? "BEEP OFF" : "BEEP ON");
    } else {
      SerialDBG.println("Usage: beep on|off");
    }
    dbg_unlock();
  }

  // ----- DIP -----
  else if (strcmp(cmd, "dip") == 0) {
    uint8_t v = dip_read_raw();
    uint8_t dev_id = dip_dev_id();
    uint32_t baud = dip_baud_value();
    dbg_lock();
    SerialDBG.print("DIP: 0x");
    SerialDBG.print(v, HEX);
    SerialDBG.print(" DEVID=");
    SerialDBG.print(dev_id);
    SerialDBG.print(dev_id == 0 ? " (HUB)" : " (SLAVE)");
    SerialDBG.print(" BAUD=");
    SerialDBG.println(baud);
    dbg_unlock();
  }

  // ----- RS485 -----
  else if (strncmp(cmd, "rs485mon", 8) == 0) {
    char st[8];
    if (sscanf(cmd, "rs485mon %7s", st) == 1) {
      rs485_set_monitor(strcmp(st, "on") == 0);
    }
    dbg_lock();
    SerialDBG.println(rs485_monitor_enabled() ? "RS485MON ON" : "RS485MON OFF");
    dbg_unlock();
  }

  else if (strncmp(cmd, "rs485", 5) == 0) {
    char text[64] = "MBW RS485 TEST\n";
    if (sscanf(cmd, "rs485 %63[^\n]", text) == 1)
      rs485_send_str(text);
    else
      rs485_send_str("MBW RS485 TEST\n");
    dbg_lock();
    SerialDBG.println("OK");
    dbg_unlock();
  }

  else if (strcmp(cmd, "rsl") == 0) {
    bool ok = rs485_loopback();
    dbg_lock();
    SerialDBG.println(ok ? "RS485 LOOP OK" : "RS485 LOOP FAIL");
    dbg_unlock();
  }

  else if (strncmp(cmd, "baud rs485", 10) == 0) {
    unsigned long b = 0;
    bool valid = sscanf(cmd, "baud rs485 %lu", &b) == 1 && b >= 1200 && b <= 921600;
    if (valid)
      rs485_set_baud(b);
    dbg_lock();
    SerialDBG.println(valid ? "OK" : "Usage: baud rs485 <1200-921600>");
    dbg_unlock();
  }

  else if (strcmp(cmd, "baud") == 0) {
    uint32_t b = rs485_get_baud();
    dbg_lock();
    SerialDBG.print("RS485 BAUD: ");
    SerialDBG.println(b);
    dbg_unlock();
  }

  // ----- FLASH -----
  else if (strcmp(cmd, "flash") == 0) {
    flash_print_id(); // tu khoa/mo rieng (g_muSPI cho SPI, g_muSerial cho in)
  }

  else if (strcmp(cmd, "fwr") == 0) {
    flash_test_rw(); // tu khoa/mo rieng
  }

  // ----- RTC -----
  else if (strncmp(cmd, "rtc set", 7) == 0) {
    unsigned int hh, mm, ss, dd, mo, yy;
    if (sscanf(cmd, "rtc set %u/%u/%u %u:%u:%u", &dd, &mo, &yy, &hh, &mm, &ss) == 6) {
      bool okd = rtc_set_date((uint8_t)dd, (uint8_t)mo, (uint8_t)yy);
      bool okt = rtc_set_time((uint8_t)hh, (uint8_t)mm, (uint8_t)ss);
      dbg_lock();
      SerialDBG.println(okd && okt ? "OK" : "RTC FAIL");
      dbg_unlock();
    } else if (sscanf(cmd, "rtc set %u:%u:%u", &hh, &mm, &ss) == 3) {
      bool ok = rtc_set_time((uint8_t)hh, (uint8_t)mm, (uint8_t)ss);
      dbg_lock();
      SerialDBG.println(ok ? "OK" : "RTC FAIL");
      dbg_unlock();
    } else {
      dbg_lock();
      SerialDBG.println("Usage: rtc set [dd/mm/yy] hh:mm:ss");
      dbg_unlock();
    }
  }

  else if (strcmp(cmd, "rtc") == 0) {
    rtc_print();
  }

  // ----- NET_ID (chung ca deployment, luu Flash - xem flashmem.h/muc 3.2.b) -----
  else if (strncmp(cmd, "net id", 6) == 0) {
    int id;
    bool has_arg = sscanf(cmd, "net id %d", &id) == 1;
    if (has_arg) {
      bool valid = id >= 0 && id <= 63;
      if (valid) {
        net_id_save((uint8_t)id);   // tu khoa/mo g_muSPI rieng, luu qua Flash
        rf_set_network_id((uint8_t)id); // ap dung ngay, khong can reset board
      }
      dbg_lock();
      if (valid) {
        SerialDBG.print("NET_ID=");
        SerialDBG.print(id);
        SerialDBG.println(" (da luu Flash)");
      } else {
        SerialDBG.println("Usage: net id <0-63>");
      }
      dbg_unlock();
    } else {
      uint8_t stored = net_id_load(); // tu khoa/mo g_muSPI rieng
      dbg_lock();
      if (stored == NET_ID_UNSET) {
        SerialDBG.println("NET_ID: CHUA CAU HINH - dung \"net id <0-63>\" de set");
      } else {
        SerialDBG.print("NET_ID=");
        SerialDBG.println(stored);
      }
      dbg_unlock();
    }
  }

  // ----- RF -----
  else if (strcmp(cmd, "rf id") == 0) {
    bool ok = rf_ok(); // tu khoa/mo g_muSPI rieng
    dbg_lock();
    SerialDBG.println(ok ? "RF_ID=OK (nRF24L01 present)" : "RF_ID=FAIL (not found)");
    dbg_unlock();
  }

  else if (strncmp(cmd, "rf ch", 5) == 0) {
    int ch;
    bool valid = sscanf(cmd, "rf ch %d", &ch) == 1 && ch >= 0 && ch <= 125;
    if (valid)
      rf_set_channel((uint8_t)ch); // tu khoa/mo g_muSPI rieng
    dbg_lock();
    if (valid) {
      SerialDBG.print("RF_CH=");
      SerialDBG.println(ch);
    } else {
      SerialDBG.println("Usage: rf ch <0-125>");
    }
    dbg_unlock();
  }

  else if (strncmp(cmd, "rf netid", 8) == 0) {
    int id;
    bool valid = sscanf(cmd, "rf netid %d", &id) == 1 && id >= 0 && id <= 63;
    if (valid)
      rf_set_network_id((uint8_t)id); // tu khoa/mo g_muSPI rieng
    dbg_lock();
    if (valid) {
      SerialDBG.print("RF_NETID=");
      SerialDBG.println(id);
    } else {
      SerialDBG.println("Usage: rf netid <0-63>");
    }
    dbg_unlock();
  }

  else if (strncmp(cmd, "rf tx", 5) == 0) {
    char text[64] = "MBW RF TEST";
    if (sscanf(cmd, "rf tx %63[^\n]", text) != 1)
      strcpy(text, "MBW RF TEST");
    bool ok = rf_send((const uint8_t *)text, (uint16_t)strlen(text)); // tu khoa/mo g_muSPI rieng
    dbg_lock();
    SerialDBG.println(ok ? "RF_TX=OK" : "RF_TX=FAIL (too long)");
    dbg_unlock();
  }

  else if (strcmp(cmd, "rf stat") == 0) {
    uint32_t tx, rxok, rxdup, rxcrc, rxfrag;
    rf_get_stats(&tx, &rxok, &rxdup, &rxcrc, &rxfrag);
    uint8_t ch = rf_get_channel();
    uint8_t netid = rf_get_network_id();
    uint8_t devid = rf_get_dev_id();
    uint32_t hbtx, hbrx;
    rf_get_hb_stats(&hbtx, &hbrx);
    bool link_up = rf_link_up();
    uint8_t peer = rf_link_peer_id();
    uint32_t age_ms = rf_link_age_ms();
    uint16_t loss = rf_get_loss_permille();
    uint8_t redund = rf_get_redundancy();
    bool repeater = rf_is_repeater();
    uint32_t relay_cnt = rf_get_relay_cnt();

    dbg_lock();
    SerialDBG.print("RF_TX=");
    SerialDBG.print(tx);
    SerialDBG.print(" RF_RX_OK=");
    SerialDBG.print(rxok);
    SerialDBG.print(" RF_RX_DUP=");
    SerialDBG.print(rxdup);
    SerialDBG.print(" RF_RX_CRCERR=");
    SerialDBG.print(rxcrc);
    SerialDBG.print(" RF_RX_FRAGDROP=");
    SerialDBG.print(rxfrag);
    SerialDBG.print(" RF_CH=");
    SerialDBG.print(ch);
    SerialDBG.print(" RF_NETID=");
    SerialDBG.print(netid);
    SerialDBG.print(" RF_DEVID=");
    SerialDBG.println(devid);

    // ----- Link health (heartbeat) TOAN MANG - giong thong so "link
    // quality"/"RSSI" cua bo telemetry: LINK UP/DOWN, dev_id nghe thay gan
    // nhat, thoi gian tu lan nghe cuoi, ty le mat uoc luong va do du phong
    // (redundant TX) dang tu dieu chinh. Xem "rf devices"/"rf dev <id>" de
    // biet CHINH XAC tung dev_id dang UP/DOWN (quan trong khi co N>1 slave). -----
    SerialDBG.print("RF_LINK=");
    SerialDBG.print(link_up ? "UP" : "DOWN");
    SerialDBG.print(" LAST_DEV_ID=");
    SerialDBG.print(peer);
    SerialDBG.print(" AGE_MS=");
    SerialDBG.print(age_ms);
    SerialDBG.print(" LOSS_PROMILLE=");
    SerialDBG.print(loss);
    SerialDBG.print(" REDUND=");
    SerialDBG.print(redund);
    SerialDBG.print(" HB_TX=");
    SerialDBG.print(hbtx);
    SerialDBG.print(" HB_RX=");
    SerialDBG.print(hbrx);
    SerialDBG.print(" REPEATER=");
    SerialDBG.print(repeater ? "ON" : "OFF");
    SerialDBG.print(" RELAY=");
    SerialDBG.println(relay_cnt);

    // ----- Breakdown TUNG dev_id ngay tren dong status (khong con phai go
    // "rf devices" rieng): voi N slave, dong RF_LINK= toan cuc luon UP neu con
    // nghe duoc BAT KY ai - dong nay chi ro THIET BI NAO dang UP/DOWN + so giay
    // ke tu lan nghe cuoi. Bo qua dev_id cua chinh minh (khong tu nghe minh). -----
    uint8_t self_id = rf_get_dev_id();
    SerialDBG.print("RF_DEVS:");
    bool any = false;
    for (uint16_t i = 0; i < 64; i++) {
      uint8_t id = (uint8_t)i;
      if (id == self_id || !rf_dev_seen(id))
        continue;
      any = true;
      SerialDBG.print(' ');
      SerialDBG.print(id);
      SerialDBG.print(rf_dev_link_up(id) ? ":UP(" : ":DOWN(");
      SerialDBG.print(rf_dev_age_s(id));
      SerialDBG.print("s,");
      {
        uint16_t dl = rf_dev_loss_permille(id); // 0xFFFF = chua du du lieu
        if (dl == 0xFFFF)
          SerialDBG.print("-");
        else
          SerialDBG.print(dl); // phan nghin (%o): TOT <20, CHAP NHAN 20-100, KEM >100
      }
      SerialDBG.print(")");
    }
    if (!any)
      SerialDBG.print(" (chua nghe thay thiet bi nao)");
    SerialDBG.println();
    dbg_unlock();
  }

  else if (strcmp(cmd, "rf reset") == 0) {
    rf_reset_stats();
    dbg_lock();
    SerialDBG.println("OK (da dat lai ca cua so do LOSS%o theo dev_id - cho >=60s roi doc 'rf devices')");
    dbg_unlock();
  }

  // ----- Redundant TX CO DINH (2026-07-06): doi so lan gui lap khi bench
  // test - chi RAM, ve mac dinh RF_REDUNDANT_TX_DEFAULT sau reset. -----
  else if (strncmp(cmd, "rf redund", 9) == 0) {
    int n;
    bool has_arg = sscanf(cmd, "rf redund %d", &n) == 1;
    bool valid = has_arg && n >= RF_REDUNDANT_TX_MIN && n <= RF_REDUNDANT_TX_MAX;
    if (valid)
      rf_set_redundancy((uint8_t)n);
    uint8_t cur = rf_get_redundancy();
    dbg_lock();
    if (has_arg && !valid) {
      SerialDBG.println("Usage: rf redund <2-6>");
    } else {
      SerialDBG.print("RF_REDUND=");
      SerialDBG.print(cur);
      SerialDBG.println(" (co dinh, chi RAM - mac dinh 3 sau reset)");
    }
    dbg_unlock();
  }

  // ----- REPEATER 1-hop (muc 6 tai lieu dinh huong): bat cho DUNG 1 board
  // trung gian giua hub va slave xa, SAU KHI da toi uu vat ly (kenh/anten/vi
  // tri) ma LOSS%o van cao (quy trinh muc 7.3). Luu Flash - giu qua mat nguon. -----
  else if (strncmp(cmd, "rf repeater", 11) == 0) {
    char st[8];
    bool has_arg = sscanf(cmd, "rf repeater %7s", st) == 1;
    bool valid = has_arg && (strcmp(st, "on") == 0 || strcmp(st, "off") == 0);
    if (valid) {
      bool en = (strcmp(st, "on") == 0);
      rf_set_repeater(en);
      repeater_save(en); // tu khoa/mo g_muSPI rieng - TRUOC dbg_lock() (thu tu khoa)
    }
    bool cur = rf_is_repeater();
    dbg_lock();
    if (has_arg && !valid) {
      SerialDBG.println("Usage: rf repeater on|off");
    } else {
      SerialDBG.print("REPEATER=");
      SerialDBG.print(cur ? "ON" : "OFF");
      SerialDBG.println(valid ? " (da luu Flash)" : "");
      if (cur)
        SerialDBG.println("LUU Y: chi nen co DUNG 1 repeater/khu vuc (dung 'rf devices' kiem tra); "
                           "tang timeout Modbus master (>=1500ms) de co bien cho do tre them 1 hop");
    }
    dbg_unlock();
  }

  // ----- RF theo tung dev_id (muc 3.2.e/7.2) - dung de do trung lap dev_id /
  // thieu Hub luc nghiem thu, va biet CHINH XAC slave nao dang mat song. -----
  else if (strcmp(cmd, "rf devices") == 0) {
    dbg_lock();
    // LOSS%o rieng tung thiet bi (muc 7.2): so voi nguong MBW_Test_Procedure
    // TOT <20%o / CHAP NHAN 20-100%o / KEM >100%o. "-" = chua du du lieu
    // (cho >=10s sau "rf reset"). Quy trinh do: cap nguon du board -> rf reset
    // -> cho >=60s -> doc bang nay.
    SerialDBG.println("dev_id  role   link  age_s  loss%o");
    for (uint16_t i = 0; i < 64; i++) {
      uint8_t id = (uint8_t)i;
      if (!rf_dev_seen(id))
        continue;
      uint16_t dloss = rf_dev_loss_permille(id);
      SerialDBG.print(id < 10 ? "  " : " ");
      SerialDBG.print(id);
      SerialDBG.print(id == 0 ? "     HUB  " : "   SLAVE  ");
      SerialDBG.print(rf_dev_link_up(id) ? "UP    " : "DOWN  ");
      SerialDBG.print(rf_dev_age_s(id));
      SerialDBG.print("     ");
      if (dloss == 0xFFFF)
        SerialDBG.println("-");
      else
        SerialDBG.println(dloss);
    }
    dbg_unlock();
  }

  else if (strncmp(cmd, "rf dev", 6) == 0) {
    int id;
    bool valid = sscanf(cmd, "rf dev %d", &id) == 1 && id >= 0 && id <= 63;
    dbg_lock();
    if (!valid) {
      SerialDBG.println("Usage: rf dev <0-63>");
    } else if (!rf_dev_seen((uint8_t)id)) {
      SerialDBG.print("DEV_ID=");
      SerialDBG.print(id);
      SerialDBG.println(" CHUA TUNG NGHE THAY");
    } else {
      uint16_t dloss = rf_dev_loss_permille((uint8_t)id);
      SerialDBG.print("DEV_ID=");
      SerialDBG.print(id);
      SerialDBG.print(id == 0 ? " (HUB)" : " (SLAVE)");
      SerialDBG.print(" LINK=");
      SerialDBG.print(rf_dev_link_up((uint8_t)id) ? "UP" : "DOWN");
      SerialDBG.print(" AGE_S=");
      SerialDBG.print(rf_dev_age_s((uint8_t)id));
      SerialDBG.print(" LOSS_PROMILLE=");
      if (dloss == 0xFFFF)
        SerialDBG.println("- (chua du du lieu, cho >=10s sau 'rf reset')");
      else
        SerialDBG.println(dloss);
    }
    dbg_unlock();
  }

  // ----- EVENT LOG (su kien DOWN/UP tung dev_id + gio RTC + loss%, luu Flash) -----
  else if (strcmp(cmd, "log clear") == 0) {
    flashlog_clear();
    dbg_lock();
    SerialDBG.println("LOG: da xoa toan bo");
    dbg_unlock();
  }
  else if (strcmp(cmd, "log") == 0 || strcmp(cmd, "log dump") == 0) {
    uint16_t n = flashlog_count();
    dbg_lock();
    SerialDBG.print("LOG_BEGIN n=");
    SerialDBG.println(n);
    dbg_unlock();
    for (uint16_t i = 0; i < n; i++) {
      uint8_t r[FLASHLOG_REC_SIZE];
      if (!flashlog_read(i, r))
        break;
      dbg_lock();
      SerialDBG.print("LOG: ");
      SerialDBG.print(i);
      SerialDBG.print(' ');
      if (r[6] < 10) SerialDBG.print('0');
      SerialDBG.print(r[6]); // day
      SerialDBG.print('/');
      if (r[7] < 10) SerialDBG.print('0');
      SerialDBG.print(r[7]); // month
      SerialDBG.print(' ');
      if (r[0] < 10) SerialDBG.print('0');
      SerialDBG.print(r[0]);
      SerialDBG.print(':');
      if (r[1] < 10) SerialDBG.print('0');
      SerialDBG.print(r[1]);
      SerialDBG.print(':');
      if (r[2] < 10) SerialDBG.print('0');
      SerialDBG.print(r[2]);
      SerialDBG.print(" dev=");
      SerialDBG.print(r[3]);
      SerialDBG.print(r[4] == FLASHLOG_EVT_UP ? " UP" : " DOWN");
      SerialDBG.print(" loss=");
      if (r[5] == 0xFF)
        SerialDBG.println("-");
      else {
        SerialDBG.print(r[5]);
        SerialDBG.println("%");
      }
      dbg_unlock();
    }
    dbg_lock();
    SerialDBG.print("LOG_END n=");
    SerialDBG.println(n);
    dbg_unlock();
  }

  // ----- BRIDGE (chuc nang MAC DINH: cau RS485 <-> Wireless) -----
  else if (strncmp(cmd, "bridge log", 10) == 0) {
    char st[8];
    if (sscanf(cmd, "bridge log %7s", st) == 1 &&
        (strcmp(st, "on") == 0 || strcmp(st, "off") == 0)) {
      bridge_set_log(strcmp(st, "on") == 0);
    } else {
      dbg_lock();
      SerialDBG.println(bridge_log_enabled() ? "BRIDGE LOG: ON" : "BRIDGE LOG: OFF");
      dbg_unlock();
    }
  }

  else if (strncmp(cmd, "bridge", 6) == 0) {
    char st[8];
    if (sscanf(cmd, "bridge %7s", st) == 1) {
      if (strcmp(st, "on") == 0) {
        bridge_set_enable(true);
      } else if (strcmp(st, "off") == 0) {
        bridge_set_enable(false);
      } else if (strcmp(st, "stat") == 0) {
        uint32_t a, b, dropa, dropb;
        bridge_get_stats(&a, &b);
        bridge_get_drop_stats(&dropa, &dropb);
        bool en = bridge_is_enabled();
        bool logen = bridge_log_enabled();
        dbg_lock();
        SerialDBG.print("BRIDGE=");
        SerialDBG.print(en ? "ON" : "OFF");
        SerialDBG.print(" LOG=");
        SerialDBG.print(logen ? "ON" : "OFF");
        SerialDBG.print(" RS485_TO_RF=");
        SerialDBG.print(a);
        SerialDBG.print(" RF_TO_RS485=");
        SerialDBG.print(b);
        SerialDBG.print(" DROP_RS485_TO_RF=");
        SerialDBG.print(dropa);
        SerialDBG.print(" DROP_RF_TO_RS485=");
        SerialDBG.println(dropb);
        dbg_unlock();
      } else if (strcmp(st, "reset") == 0) {
        bridge_reset_stats();
        dbg_lock();
        SerialDBG.println("OK");
        dbg_unlock();
      } else {
        dbg_lock();
        SerialDBG.println("Usage: bridge on|off|log <on|off>|stat|reset");
        dbg_unlock();
      }
    } else {
      dbg_lock();
      SerialDBG.println("Usage: bridge on|off|log <on|off>|stat|reset");
      dbg_unlock();
    }
  }

  // ----- RTOS: kiem tra RAM/stack con lai THUC TE tren phan cung (chip chi
  // 10KB RAM) - dung khi tinh chinh configTOTAL_HEAP_SIZE/stack tung task. -----
  else if (strcmp(cmd, "rtos stat") == 0) {
    size_t free_heap = xPortGetFreeHeapSize();
    UBaseType_t hw_rs485 = g_hTaskRS485 ? uxTaskGetStackHighWaterMark(g_hTaskRS485) : 0;
    UBaseType_t hw_rf = g_hTaskRF ? uxTaskGetStackHighWaterMark(g_hTaskRF) : 0;
    UBaseType_t hw_cli = g_hTaskCLI ? uxTaskGetStackHighWaterMark(g_hTaskCLI) : 0;

    dbg_lock();
    SerialDBG.print("RTOS_FREE_HEAP_BYTES=");
    SerialDBG.println((unsigned long)free_heap);
    // uxTaskGetStackHighWaterMark tra ve so WORD con TRONG (chua bao gio dung
    // toi) - cang gan 0 nghia la stack task do cang sat gioi han, can tang
    // RTOS_STACK_xxx trong rtos_glue.h.
    SerialDBG.print("STACK_FREE_WORDS RS485=");
    SerialDBG.print((unsigned long)hw_rs485);
    SerialDBG.print(" RF=");
    SerialDBG.print((unsigned long)hw_rf);
    SerialDBG.print(" CLI=");
    SerialDBG.println((unsigned long)hw_cli);
    dbg_unlock();
  }

  // ----- WATCHDOG (IWDG, xem drivers/watchdog.h + rtos_glue.h) -----
  else if (strcmp(cmd, "wdt stat") == 0) {
    bool boot_was_reset = wdt_boot_was_reset();
    bool all_alive = rtos_all_tasks_alive();
    uint32_t now = millis();
    uint32_t age_rs485 = now - g_aliveRS485Ms;
    uint32_t age_rf = now - g_aliveRFMs;
    uint32_t age_cli = now - g_aliveCLIMs;
    dbg_lock();
    SerialDBG.print("WDT_BOOT_WAS_RESET=");
    SerialDBG.println(boot_was_reset ? "YES (lan truoc bi treo!)" : "no");
    SerialDBG.print("WDT_FEEDING=");
    SerialDBG.println(all_alive ? "YES (ca 3 task deu song)" : "NO (co task dang treo - sap RESET!)");
    SerialDBG.print("TASK_ALIVE_AGE_MS RS485=");
    SerialDBG.print(age_rs485);
    SerialDBG.print(" RF=");
    SerialDBG.print(age_rf);
    SerialDBG.print(" CLI=");
    SerialDBG.println(age_cli);
    dbg_unlock();
  }

  else {
    dbg_lock();
    SerialDBG.println("ERR");
    dbg_unlock();
  }
}

void cli_process() {
  while (SerialDBG.available()) {
    char c = SerialDBG.read();

    if (c == '\r' || c == '\n') {
      if (cmd_idx > 0) {
        cmd_buf[cmd_idx] = 0;
        cli_execute(cmd_buf);
        cmd_idx = 0;
      }
    } else if (cmd_idx < CLI_BUF_SIZE - 1) {
      cmd_buf[cmd_idx++] = c;
    }
  }
}
