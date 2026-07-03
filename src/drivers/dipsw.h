#pragma once
#include <Arduino.h>

// ID DIP Switch 8-bit (S1) doc qua 74HC165 (SH/LD=PC13, CLK=PC14, QH=PC15).
// Theo tai lieu san pham (epcb.vn):
//   SW1-SW6 = Network ID (0-63, 64 mang Modbus khac nhau)
//   SW7-SW8 = Baudrate RS485: 00=4800, 01=9600, 10=14400, 11=19200
// LUU Y: thu tu bit vat ly (SW1..SW8 <-> A..H cua 74HC165) CAN doi chieu tren
// ban test that (netlist schematic bi non khi trich xuat). ham dip_read_raw()
// tra ve nguyen 8 bit theo thu tu doc duoc tu QH; dip_network_id()/dip_baud_sel()
// gia dinh bit0..bit5 = SW1..SW6, bit6..bit7 = SW7..SW8 - SUA lai o dipsw.cpp
// neu do dac thuc te khac.

void dip_init();

uint8_t dip_read_raw(); // 8-bit tho, bit=1 khi switch o vi tri ON

uint8_t dip_network_id(); // 0-63, tu SW1-6
uint8_t dip_baud_sel();   // 0-3, tu SW7-8
uint32_t dip_baud_value(); // gia tri baud thuc (4800/9600/14400/19200)

void dip_process(); // goi trong loop(): poll + in "DIP: 0xNN" khi thay doi
