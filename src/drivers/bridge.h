#pragma once
#include <Arduino.h>

// =========================================================
// bridge.* = CHUC NANG MAC DINH cua san pham: cau noi RS485 <-> Wireless.
// MAC DINH BAT NGAY TU LUC KHOI DONG (bridge_init() -> enabled = true) - day
// la hanh vi binh thuong cua thiet bi khi lap dat that, KHONG can lenh CLI
// nao de kich hoat. Lenh "bridge on|off" chi dung de KY THUAT VIEN tam tat
// khi can chay rieng cac test CLI khac (rs485 <text>, rf tx <text>...) ma
// khong muon bridge tranh doc RS485/RF.
//
// Nhan byte tu RS485 (phat hien het khung theo khoang lang ~3.5 ky tu, kieu
// Modbus RTU), goi qua RF (rf_send, co lap lai + CRC rieng); chieu nguoc lai
// nhan RF (rf_available/rf_read) va day ra RS485 that (rs485_send_start()/
// rs485_send_poll() - NON-BLOCKING, xem ghi chu 2026-07-07 lan 2 duoi day).
//
// Khi bridge dang BAT: bridge_rs485_step() la noi DUY NHAT doc RS485 RX (KHONG
// duoc bat dong thoi voi "rs485mon"/rs485_process() - se tranh doc byte).
//
// LOG FORWARD: moi lan relay 1 khung (ca 2 chieu) se in 1 dong ngan ra console
// dang "FWD RS485->RF: <n> bytes: <preview>" / "FWD RF->RS485: <n> bytes: ...".
// App test doc dong nay de HIEN THI TRUC QUAN hoat dong forward tren man
// hinh (2 board cam vao 2 may tinh khac nhau, moi may chay 1 app rieng quan
// sat board cua minh). Mac dinh TAT, bat bang "bridge log on" khi can test.
//
// 2026-07-07 (lan 1): viec IN (SerialDBG.print) da duoc TACH KHOI task RS485 -
// task RS485 chi enqueue khong-block vao 1 ring buffer nho tu viet tay (bien
// tinh s_log_buf/s_log_head/s_log_tail trong bridge.cpp - KHONG dung FreeRTOS
// queue vi StaticQueue_t qua dat RAM tren chip 10KB, xem chi tiet trong
// bridge.cpp), task CLI (uu tien thap nhat) moi thuc su in qua
// bridge_log_process() (goi tu main.cpp moi vong lap).
//
// 2026-07-07 (lan 2): do lai 30 phut sau fix lan 1 cho thay tan suat "ghep 2
// khung lam 1" KHONG doi (~39.9 lan/gio truoc vs ~39.7 lan/gio sau) - chung to
// fix lan 1 KHONG phai nguyen nhan chinh. Nguyen nhan THAT SU: rs485_send()
// (chieu RF->RS485, goi ngay trong bridge_rs485_step()) BLOCK cho toi khi
// flush() xong THAT SU (vai ms tuy do dai khung/baud) - dong bang viec tach
// khung Modbus chieu RS485->RF trong luc do. Da doi sang
// rs485_send_start()/rs485_send_poll() (non-blocking, xem rs485.h) - xem chi
// tiet trong bridge.cpp va docs/Bao_cao_mat_frame_..., muc 6-7.
//
// 2026-07-08: them adapt_redundancy_check() (bridge.cpp, goi tu bridge_rf_step()
// moi vong lap, tu kich hoat khi du >=20 mau moi - xem chi tiet trong
// bridge.cpp) - TU DONG tang/giam "rf redund" theo hieu so RS485->RF/RF->RS485
// THAT DO (chi so da chung minh phan anh dung ty le mat goi RF, xem bao cao
// muc 11), thay vi phai chinh tay. CHI CHAY TREN HUB (dev_id==0) - tren slave
// chi so nay khong dung nghia vi la mang broadcast (xem giai thich chi tiet
// trong bridge.cpp). Han che da biet: chi tu dieu chinh chieu Hub->Slave, KHONG
// giai quyet duoc mat goi chieu Slave->Hub (van phai chinh tay tren tung slave
// neu can).
//
// ===== RTOS (2026): tach lam 2 ham thay vi 1 bridge_process() duy nhat =====
// bridge_rs485_step() : goi TU TASK RS485 (uu tien cao nhat) - chi doc RS485,
//                        phat hien khoang lang gap khung, va DAY khung hoan
//                        chinh vao hang doi g_qToRF (KHONG goi rf_send() truc
//                        tiep - tranh bi khoa lai boi redundant TX cua RF).
//                        Cung lay khung tu g_qToRS485 (neu co) de day ra RS485
//                        that qua rs485_send_start()/rs485_send_poll()
//                        (NON-BLOCKING - xem ghi chu 2026-07-07 lan 2 o tren).
// bridge_rf_step()    : goi TU TASK RF (uu tien trung binh) - goi rf_process()
//                        (nhan + heartbeat), lay khung tu g_qToRF (neu co) de
//                        rf_send() thuc su, va neu rf_available() thi day khung
//                        nhan duoc vao g_qToRS485.
// Ca 2 ham deu dung xQueueSend/xQueueReceive KHONG BLOCK (timeout=0): neu hang
// doi day (do sau = 1), khung bi ROT - chap nhan duoc vi Modbus master da co
// co che timeout/retry rieng o lop tren (giong triet ly "khong dam bao khong
// mat goi o lop RF" da ap dung cho heartbeat/redundant TX).
// =========================================================

void bridge_init();

void bridge_set_enable(bool en);
bool bridge_is_enabled();

void bridge_set_log(bool en);
bool bridge_log_enabled();

void bridge_rs485_step(); // goi tu task RS485 moi vong lap
void bridge_rf_step();    // goi tu task RF moi vong lap

void bridge_get_stats(uint32_t *rs485_to_rf, uint32_t *rf_to_rs485);
void bridge_get_drop_stats(uint32_t *rs485_to_rf_drop, uint32_t *rf_to_rs485_drop);
void bridge_reset_stats();

// goi tu task CLI (uu tien THAP NHAT) moi vong lap - in cac dong "FWD ..." da
// duoc enqueue (khong block task RS485) - xem giai thich o tren.
void bridge_log_process();
