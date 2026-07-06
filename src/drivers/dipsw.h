#pragma once
#include <Arduino.h>

// ID DIP Switch 8-bit (S1) doc qua 74HC165 (SH/LD=PC13, CLK=PC14, QH=PC15).
//
// 2026-07-04 (xem docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md muc 3.2.a):
// DOI VAI TRO SW1-6 tu Network ID sang DEV_ID (cung 6 bit vat ly, khong doi
// PCB/BOM, chi doi cach firmware dien giai):
//   SW1-SW6 = DEV_ID (0-63): 0 = HUB (DIP de nguyen, khong gat bit nao),
//             1-63 = SLAVE - DOC LAP HOAN TOAN voi dia chi Modbus that cua
//             thiet bi slave (ky thuat vien tu ghi lai 2 con so rieng luc lap
//             dat: dia chi Modbus that + vi tri DIP dev_id).
//   SW7-SW8 = Baudrate RS485: 00=4800, 01=9600, 10=14400, 11=19200
// Network ID (hang so CHUNG cho ca deployment, khong con doc tu DIP nua) da
// chuyen sang cau hinh qua CLI "net id <n>" + luu Flash - xem flashmem.h.
//
// LUU Y: thu tu bit vat ly (SW1..SW8 <-> A..H cua 74HC165) CAN doi chieu tren
// ban test that (netlist schematic bi non khi trich xuat). ham dip_read_raw()
// tra ve nguyen 8 bit theo thu tu doc duoc tu QH; dip_dev_id()/dip_baud_sel()
// gia dinh bit0..bit5 = SW1..SW6, bit6..bit7 = SW7..SW8 - SUA lai o dipsw.cpp
// neu do dac thuc te khac.

void dip_init();

uint8_t dip_read_raw(); // 8-bit LOGIC: bit=1 khi switch o vi tri ON (ON = noi GND;
                        // phan cung doc nguoc do pull-up, da DAO BIT san ben trong -
                        // 2026-07-06, doi chieu tren board that: all-OFF = 0x00 = HUB)

uint8_t dip_dev_id();     // 0-63, tu SW1-6: 0=Hub, 1-63=Slave (RF dev_id, KHONG phai dia chi Modbus)
uint8_t dip_baud_sel();   // 0-3, tu SW7-8
uint32_t dip_baud_value(); // gia tri baud thuc (4800/9600/14400/19200)

void dip_process(); // goi trong loop(): poll + in "DIP: 0xNN" khi thay doi
