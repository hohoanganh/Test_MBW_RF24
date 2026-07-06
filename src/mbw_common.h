#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// =========================================================
// MBW RF24 RS485 2.0 (MBW-RF-00-24) - pin map & hang so chung
// Nguon: docs/MBW_RF24_RS485_2I0_RevB_Hardware.md + PinMap.md
// (trich schematic MBW_RF24_RS485_2I0_RevB.PrjPcb, rev B)
// =========================================================

// ===== FIRMWARE VERSION =====
// 0.3.0 (2026-07-06): EVENT LOG mat-link theo dev_id luu Flash (vung 0x010000,
//   2048 ban ghi, KHONG ton RAM tinh - quet vi tri ghi on-demand vi chip 10KB);
//   moi su kien DOWN dong dau NGAY+GIO tu RTC. Mo rong RTC PCF85063 co NGAY
//   (rtc_get/set_date; "rtc set [dd/mm/yy] hh:mm:ss"). Lenh moi: "log"/"log clear".
//   Dong "rf stat" them RF_DEVS: liet ke tung dev_id UP/DOWN + loss ngay tren
//   status. Event UP/DOWN theo dev_id in noi bat hon (nhan HUB/SLAVE).
// 0.2.0 (2026-07-06): toi uu lap dat 1 hub + 16 slave - redundant TX co dinh
// (bo auto-adapt toan cuc), LOSS%o theo tung dev_id, REPEATER 1-hop (byte hop
// trong khung -> DOI CAU TRUC KHUNG, moi board phai nap cung phien ban nay),
// bridge log mac dinh OFF. Xem README +
// docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md.
#define FW_VERSION "0.3.0"

// ===== DEVICE ID (van tay de app test xac thuc dung thiet bi) =====
#define DEVICE_ID "MBW_RF24_RS485"

// ===== BAUDRATE =====
#define DBG_BAUD 115200 // Console USART1, CO DINH
#define RS485_BAUD_DEFAULT 9600

// ===== CONSOLE (USART1) =====
#define UART_DBG_TX PA9
#define UART_DBG_RX PA10

// ===== RS485 (USART2) =====
#define RS485_TX PA2
#define RS485_RX PA3
#define RS485_DIR PA1 // dieu khien DE/RE THVD1420 (HIGH = truyen)

// ===== SPI1 (dung chung: nRF24L01 + W25Q128 Flash) =====
#define SPI_SCK PA5
#define SPI_MISO PA6
#define SPI_MOSI PA7
#define FLASH_CS PB14

// ===== nRF24L01P + RFX2401C (RF front-end PA/LNA) =====
#define RF_CE PA8
#define RF_CSN PB9
#define RF_IRQ PB1
// TXEN/RXEN cua RFX2401C dau cung PA8/RF_CE tren schematic (R6/R11) -> PA/LNA
// tu chuyen mach theo CE, KHONG can GPIO rieng tu firmware. Neu do dac tren
// bo mau thay TXEN/RXEN khong bam theo CE nhu ky vong, can bo sung GPIO dieu
// khien rieng trong rf_link.cpp (TODO xac nhan tren ban test).

// ===== ID DIP Switch (8-bit, doc qua 74HC165 - khong dung CLK_INH) =====
#define SW_LOAD PC13 // SH/LD (chot du lieu song song)
#define SW_SCK PC14  // CLK (xung dich)
#define SW_MISO PC15 // QH (du lieu noi tiep ve MCU, MSB truoc)

// ===== RTC PCF85063 (I2C1: PB6=SCL, PB7=SDA), dia chi 0x51 =====
#define RTC_ADDR 0x51

// ===== LED, Buzzer, nut nhan =====
#define LED_LIFE PB8 // LED trang thai 2 mau (qua Q4)
#define BUZZER PB0   // qua Q1, active-high
#define BUZZER_ON_LEVEL HIGH
#define BUZZER_OFF_LEVEL LOW
#define USER_BTN PA11 // nut nhan S2, INPUT_PULLUP (khong phai NRST)

// ===== SWD (thong tin, khong dung lam GPIO trong firmware) =====
// PA13 = SWDIO, PA14 = SWCLK

extern HardwareSerial SerialDBG;   // USART1 - console
extern HardwareSerial SerialRS485; // USART2 - RS485
