#include "opms_common.h"
#include "hal.h"
#include "opms_drv.h"

// Console serial khai bao o main.cpp
extern HardwareSerial SerialDBG;

// =====================================================================
//  OPMS 1.6 - HAL implementation (buoc 1)
// =====================================================================

static bool s_led_life = false;
static uint32_t s_buzzer_off_at = 0;
static bool s_buzzer_on = false;

void hal_init() {
  // --- Console USART1 (PA9/PA10) ---
  SerialDBG.begin(DBG_BAUD);

  // --- LED / Buzzer ---
  pinMode(LED_LIFE, OUTPUT);
  pinMode(LED_RUN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(LED_LIFE, LOW);
  digitalWrite(LED_RUN, LOW);
  digitalWrite(BUZZER, LOW);   // buzzer active-high -> LOW = tat
}

void led_life_toggle() {
  s_led_life = !s_led_life;
  digitalWrite(LED_LIFE, s_led_life ? HIGH : LOW);
}

void led_run_set(bool on) {
  digitalWrite(LED_RUN, on ? HIGH : LOW);
}

void buzzer_beep(uint16_t ms) {
  digitalWrite(BUZZER, HIGH);
  s_buzzer_on = true;
  s_buzzer_off_at = millis() + ms;
}

void buzzer_update() {
  if (s_buzzer_on && (int32_t)(millis() - s_buzzer_off_at) >= 0) {
    digitalWrite(BUZZER, LOW);
    s_buzzer_on = false;
  }
}

void uart_log(const char *msg) {
  SerialDBG.println(msg);
}

void cli_prompt() {
  SerialDBG.print("OPMS> ");
}

// =====================================================================
//  CLI - xu ly lenh tu Console (buoc 1: id/ver/help/led/beep)
// =====================================================================
static char s_buf[64];
static uint8_t s_len = 0;

// In ra dang KEY=VALUE (app de parse)
static void kv(const char *key, long val) {
  SerialDBG.print(key);
  SerialDBG.print('=');
  SerialDBG.println(val);
}

static bool is_on(const char *s) {
  return (strcmp(s, "on") == 0 || strcmp(s, "1") == 0);
}

static void cli_help() {
  uart_log("=== Lenh OPMS 1.6 ===");
  uart_log(" id | ver | bver | help | led | beep");
  uart_log(" gpo <1-4> <on|off>   | gpoi            (dong ngo ra mA)");
  uart_log(" fan <1-4> <on|off> [pct] | fanr <1-4>  (Quat 48V, % + rpm)");
  uart_log(" gpi                                    (8 dau vao)");
  uart_log(" ac <on|off>          | aci             (dong AC mA)");
  uart_log(" dfan <1-3> <on|off>  | dfanfb/dfanr <1-2> (Quat 12V: fb/rpm)");
  uart_log(" flash                                  (W25Q80 ID)");
  uart_log(" --- Cam bien / Nguon / RS485 ---");
  uart_log(" ntc <1-4>            | hum <1-2>       (gia tri 0-255)");
  uart_log(" humpwr <1-2> <on|off>                  (nguon 5V cam bien)");
  uart_log(" rspwr <on|off>       | rspg            (nguon 12V RS485)");
  uart_log(" rs485 <1-4> [text]   | rs485mon <1-4>   (gui / monitor NEN)");
  uart_log(" rs485loop <1-4>      | rs485stop        (txloop NEN / dung monitor+txloop)");
  uart_log(" rs485tx <1-4> [text] | rs485dir <1-4> <0|1>  (gui lap / giu DIR de do)");
  uart_log(" pi                                     (loopback UART Orange Pi)");
}

static void cli_exec(char *cmd) {
  // tach token
  char *argv[4]; int argc = 0;
  char *p = cmd;
  while (*p && argc < 4) {
    while (*p == ' ') p++;
    if (!*p) break;
    argv[argc++] = p;
    while (*p && *p != ' ') p++;
    if (*p) { *p = 0; p++; }
  }
  if (argc == 0) return;
  const char *c = argv[0];

  if (strcmp(c, "id") == 0) {
    uart_log(DEVICE_ID);
  } else if (strcmp(c, "ver") == 0) {
    uart_log("OPMS 1.6 SLAVE FW " FW_VERSION);
  } else if (strcmp(c, "bver") == 0) {
    kv("BOARD_VER", board_version_read());
  } else if (strcmp(c, "help") == 0) {
    cli_help();
  } else if (strcmp(c, "led") == 0) {
    static bool on = false; on = !on; led_run_set(on);
    uart_log(on ? "LED_RUN=1" : "LED_RUN=0");
  } else if (strcmp(c, "beep") == 0) {
    buzzer_beep(150); uart_log("BEEP");

  // ---- GPO ----
  } else if (strcmp(c, "gpo") == 0 && argc >= 3) {
    int ch = atoi(argv[1]);
    gpo_set((uint8_t)ch, is_on(argv[2]));
    SerialDBG.print("OK GPO"); SerialDBG.print(ch);
    SerialDBG.println(is_on(argv[2]) ? "=ON" : "=OFF");
  } else if (strcmp(c, "gpoi") == 0) {
    kv("GPO_CUR", gpo_current_mA());

  // ---- Quat 48V ----  (fan <ch> <on|off> [pct])
  } else if (strcmp(c, "fan") == 0 && argc >= 3) {
    int ch = atoi(argv[1]);
    bool on = is_on(argv[2]);
    int pct = (argc >= 4) ? atoi(argv[3]) : 100;
    if (on) fan_set_pwm((uint8_t)ch, (uint8_t)pct);
    else    fan_set((uint8_t)ch, false);
    SerialDBG.print("OK FAN"); SerialDBG.print(ch);
    if (on) { SerialDBG.print("="); SerialDBG.print(pct); SerialDBG.println("%"); }
    else    SerialDBG.println("=OFF");
  } else if (strcmp(c, "fanr") == 0 && argc >= 2) {
    int ch = atoi(argv[1]);
    char key[12]; snprintf(key, sizeof(key), "FAN%d_RPS", ch);
    kv(key, fan_rps((uint8_t)ch));

  // ---- GPI ----
  } else if (strcmp(c, "gpi") == 0) {
    uint8_t v = gpi_read_all();
    kv("GPI", v);   // gia tri 8-bit (bit0 = GPI1)

  // ---- Relay AC ----
  } else if (strcmp(c, "ac") == 0 && argc >= 2) {
    ac_relay_set(is_on(argv[1]));
    SerialDBG.println(is_on(argv[1]) ? "OK AC=ON" : "OK AC=OFF");
  } else if (strcmp(c, "aci") == 0) {
    kv("AC_CUR", ac_current_mA());

  // ---- DEV fan ----
  } else if (strcmp(c, "dfan") == 0 && argc >= 3) {
    int ch = atoi(argv[1]);
    dev_fan_set((uint8_t)ch, is_on(argv[2]));
    SerialDBG.print("OK DFAN"); SerialDBG.print(ch);
    SerialDBG.println(is_on(argv[2]) ? "=ON" : "=OFF");
  } else if (strcmp(c, "dfanfb") == 0 && argc >= 2) {
    int ch = atoi(argv[1]);
    char key[12]; snprintf(key, sizeof(key), "DFAN%d_FB", ch);
    kv(key, dev_fan_fb((uint8_t)ch) ? 1 : 0);
  } else if (strcmp(c, "dfanr") == 0 && argc >= 2) {
    int ch = atoi(argv[1]);
    char key[12]; snprintf(key, sizeof(key), "DFAN%d_RPS", ch);
    kv(key, dev_fan_rps((uint8_t)ch));

  // ---- Flash ----
  } else if (strcmp(c, "flash") == 0) {
    uint8_t m, t, cap;
    bool ok = flash_read_id(&m, &t, &cap);
    SerialDBG.print("FLASH_ID=0x"); SerialDBG.print(m, HEX);
    SerialDBG.print(" 0x"); SerialDBG.print(t, HEX);
    SerialDBG.print(" 0x"); SerialDBG.println(cap, HEX);
    SerialDBG.println(ok ? "FLASH=OK" : "FLASH=FAIL");

  // ---- Nhiet do ----
  } else if (strcmp(c, "ntc") == 0 && argc >= 2) {
    int ch = atoi(argv[1]);
    char key[8]; snprintf(key, sizeof(key), "NTC%d", ch);
    kv(key, ntc_read((uint8_t)ch));

  // ---- Do am ----
  } else if (strcmp(c, "hum") == 0 && argc >= 2) {
    int ch = atoi(argv[1]);
    char key[8]; snprintf(key, sizeof(key), "HUM%d", ch);
    kv(key, hum_read((uint8_t)ch));
  } else if (strcmp(c, "humpwr") == 0 && argc >= 3) {
    int ch = atoi(argv[1]);
    hum_power((uint8_t)ch, is_on(argv[2]));
    SerialDBG.print("OK HUM"); SerialDBG.print(ch);
    SerialDBG.println(is_on(argv[2]) ? "=ON" : "=OFF");

  // ---- Nguon 12V RS485 ----
  } else if (strcmp(c, "rspwr") == 0 && argc >= 2) {
    rs485_power(is_on(argv[1]));
    SerialDBG.println(is_on(argv[1]) ? "OK RSPWR=ON" : "OK RSPWR=OFF");
  } else if (strcmp(c, "rspg") == 0) {
    kv("RS485_PG", rs485_power_good() ? 1 : 0);

  // ---- Cong RS485 ----
  } else if (strcmp(c, "rs485") == 0 && argc >= 2) {
    // rs485 <port> [text]  -> gui chuoi (mac dinh "OPMS?") tren cong, dem byte nhan
    int port = atoi(argv[1]);
    char msg[64];
    if (argc >= 3) snprintf(msg, sizeof(msg), "%s\r\n", argv[2]);
    else           snprintf(msg, sizeof(msg), "OPMS?\r\n");
    int n = rs485_probe((uint8_t)port, msg);
    char key[12]; snprintf(key, sizeof(key), "RS485%d_RX", port);
    kv(key, n);
  } else if (strcmp(c, "rs485mon") == 0 && argc >= 2) {
    rs485_monitor((uint8_t)atoi(argv[1]));   // nghe lien tuc, go 'q' de thoat
  } else if (strcmp(c, "rs485tx") == 0 && argc >= 2) {
    int port = atoi(argv[1]);
    char msg[64];
    if (argc >= 3) snprintf(msg, sizeof(msg), "%s\r\n", argv[2]);
    else           snprintf(msg, sizeof(msg), "OPMS-TX\r\n");
    rs485_burst((uint8_t)port, msg, 20);     // gui 20 lan de soi tin hieu phat
  } else if (strcmp(c, "rs485loop") == 0 && argc >= 2) {
    rs485_txloop((uint8_t)atoi(argv[1]));    // txloop o NEN (khong block), rs485stop de dung
  } else if (strcmp(c, "rs485stop") == 0) {
    rs485_bg_stop();                         // dung monitor/txloop nen
    SerialDBG.println("RS485 bg: STOP");
  } else if (strcmp(c, "rs485dir") == 0 && argc >= 3) {
    // GIU TINH chan DIR de do bang dong ho: rs485dir <port> <0|1>
    int port = atoi(argv[1]);
    int lvl = atoi(argv[2]) ? HIGH : LOW;
    uint32_t dir = (port <= 2) ? RS485_DIR1 : RS485_DIR2;
    pinMode(dir, OUTPUT);
    digitalWrite(dir, lvl);
    SerialDBG.print("RS485_DIR cong "); SerialDBG.print(port);
    SerialDBG.println(lvl ? " = 1 (HIGH/transmit)" : " = 0 (LOW/receive)");

  // ---- Giao tiep Orange Pi (USART3) - scaffold ----
  } else if (strcmp(c, "pi") == 0) {
    SerialDBG.println(pi_selftest() ? "PI=OK" : "PI=FAIL");

  // ---- Bat/tat monitor nen Orange Pi (in "PI_RX>") ----
  } else if (strcmp(c, "pimon") == 0) {
    if (argc >= 2) pi_monitor_set(is_on(argv[1]));
    SerialDBG.print("PIMON=");
    SerialDBG.println(pi_monitor_get() ? "ON" : "OFF");

  } else {
    SerialDBG.print("Unknown cmd: ");
    SerialDBG.println(c);
  }
}

void cli_process() {
  while (SerialDBG.available()) {
    char c = (char)SerialDBG.read();

    if (c == '\r' || c == '\n') {
      // Nhan ca CR, LF hoac CRLF. Bo qua dong trong (tranh chay 2 lan voi CRLF).
      if (s_len > 0) {
        SerialDBG.println();          // echo xuong dong
        s_buf[s_len] = 0;
        cli_exec(s_buf);
        s_len = 0;
        cli_prompt();
      }
    } else if (c == 8 || c == 127) {  // Backspace / Delete
      if (s_len > 0) {
        s_len--;
        SerialDBG.print("\b \b");
      }
    } else if (s_len < sizeof(s_buf) - 1) {
      s_buf[s_len++] = c;
      SerialDBG.write(c);             // echo ky tu vua go
    }
  }
}
