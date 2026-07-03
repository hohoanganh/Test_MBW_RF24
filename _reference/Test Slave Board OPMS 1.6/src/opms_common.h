#pragma once
#include <Arduino.h>
#include <SPI.h>

// =====================================================================
//  OPMS 1.6 Slave Board - PIN MAP & CAU HINH CHUNG
//  MCU: STM32F303VCT6 (LQFP100, U17)
//  Trich tu schematic: _reference/Schematic_OPMS Slave Board 1.6.pdf
//  Chi tiet: docs/OPMS_1.6_Hardware.md , docs/OPMS_1.6_PinMap.md
//  Luu y: mot vai anh xa ADC (thu tu NTC) nen doi chieu lai truc tiep
//         tren schematic khi viet driver.
// =====================================================================

// ===== FIRMWARE VERSION =====
#define FW_VERSION "0.3.2"

// ===== DEVICE ID (van tay de app test xac thuc dung thiet bi OPMS) =====
#define DEVICE_ID "OPMS_1.6_SLAVE"

// ===== BAUDRATE =====
#define DBG_BAUD 115200     // Console USART1
#define RS485_BAUD 9600     // RS485 Slave
#define PI_UART_BAUD 115200 // USART3 <-> Orange Pi

// ===== LED / BUZZER =====
#define LED_LIFE PE7 // LED nhip (nap xong nhay = OK)
#define LED_RUN PE3  // LED trang thai chay
#define BUZZER PE0   // TMB12A05 active

// ===== CONSOLE (RS232 MAX3232 - USART1) =====
#define UART_DBG_TX PA9  // USART1_TX
#define UART_DBG_RX PA10 // USART1_RX

// ===== RS485 nhom A (USART2 -> THVD1550, cong A1/A2) =====
#define RS485A_TX PA2  // USART2_TX
#define RS485A_RX PA3  // USART2_RX
#define RS485_DIR1 PE6 // huong truyen nhom A

// ===== RS485 nhom B (UART4 -> THVD1550, cong A3/A4) =====
#define RS485B_TX PC10  // UART4_TX
#define RS485B_RX PC11  // UART4_RX
#define RS485_DIR2 PA15 // huong truyen nhom B

// ===== RS485 POWER (nguon 12V cong RS485) =====
#define RS485_PWREN PC0 // bat nguon
#define RS485_PG PC1    // power-good

// ===== UART <-> ORANGE PI (USART3) =====
#define PI_UART_TX PB10 // USART3_TX
#define PI_UART_RX PB11 // USART3_RX

// ===== SPI FLASH W25Q80 (SPI3) =====
#define SPI_SCK PB3  // SPI3_SCK
#define SPI_MISO PB4 // SPI3_MISO
#define SPI_MOSI PB5 // SPI3_MOSI
#define FLASH_CS PD7 // chip-select W25Q80

// ===== GPI - 8 dau vao 48V cach ly, doc qua 74HC165 =====
#define GPI_LOAD PA11 // chot du lieu (SH/LD)
#define GPI_EN PA12   // CLK_INH (cho phep clock)
#define GPI_CLK PB6   // clock dich
#define GPI_DATA PB7  // du lieu noi tiep ve MCU
#define GPI_COUNT 8

// ===== GPO - 4 ngo ra MOSFET =====
#define GPO1 PD9
#define GPO2 PD10
#define GPO3 PD11
#define GPO4 PD12
#define GPO_CUR PB13    // ADC: do dong tong (ACS712)
#define GPO_OFFSET PB12 // ADC: offset/Vref do dong

// ===== QUAT 48V (4 kenh): PWM ra + TACH ve =====
#define FAN1_PWM PC6
#define FAN2_PWM PC7
#define FAN3_PWM PC8
#define FAN4_PWM PC9
#define FAN1_TACH PD6
#define FAN2_TACH PD5
#define FAN3_TACH PD4
#define FAN4_TACH PD3

// ===== QUAT NHO THIET BI (DEV FAN): control + feedback =====
#define DEV_FAN1_CTR PB8
#define DEV_FAN1_FB PB9
#define DEV_FAN2_CTR PC12
#define DEV_FAN2_FB PD0
#define DEV_FAN3_CTR PD1
#define DEV_FAN3_FB PD2

// ===== RELAY AC (Air Condition - G2RL) =====
#define ACC_CTR PE5 // dieu khien relay AC
#define ADC_CT PB0  // ADC: do dong relay AC

// ===== CAM BIEN NHIET DO (NTC 10K, 4 cong - ADC) =====
// TODO doi chieu: tu netlist NTC1=PA5, NTC2=PA4, NTC3=PA6, NTC4=PA7
#define NTC1 PA5
#define NTC2 PA4
#define NTC3 PA6
#define NTC4 PA7

// ===== CAM BIEN DO AM / RHT (2 cong) =====
#define HUM1_DATA PC4 // ADC
#define HUM2_DATA PC5 // ADC
#define HUM1_EN PE10  // cap nguon 5V cam bien 1
#define HUM2_EN PE15  // cap nguon 5V cam bien 2

// ===== VERSION CONFIG (3-bit, 011 = v1.6) =====
#define VER_CFG0 PD13
#define VER_CFG1 PD14
#define VER_CFG2 PD15

// ===== SWD (nap/debug) =====
#define SWDIO PA13
#define SWCLK PA14

// ===== GLOBAL SERIAL OBJECTS (khai bao o main.cpp) =====
// extern HardwareSerial SerialDBG;     // Console USART1
// extern HardwareSerial SerialRS485A;  // USART2
// extern HardwareSerial SerialRS485B;  // UART4
// extern HardwareSerial SerialPI;      // USART3
