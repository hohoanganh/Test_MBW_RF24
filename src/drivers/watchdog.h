#pragma once
#include <Arduino.h>

// =========================================================
// IWDG (Independent Watchdog) cua STM32L151C8T6 - chay bang LSI noi bo
// (~37kHz), KHONG phu thuoc dong ho chinh (HSE/HSI) nen van chay dung ke ca
// khi thach anh ngoai loi. Muc dich: neu 1 trong 3 task RTOS (RS485/RF/CLI,
// xem main.cpp) bi TREO THAT SU (deadlock mutex, cho SPI vo han, ket trong
// vong lap gui RF do phan cung loi...), firmware truoc day se dung im vo han
// (chi co vApplicationStackOverflowHook() nhay LED, KHONG tu reset) - nay
// IWDG se tu dong RESET MCU sau khi het thoi gian timeout neu khong duoc
// "cho an" (feed/reload) dung han.
//
// THIET KE (2026-07-05, xem rtos_glue.h): watchdog theo SUC KHOE TUNG TASK,
// khong phai "feed vo dieu kien". Moi task tu ghi 1 moc thoi gian (millis())
// MOI VONG LAP cua no (rtos_task_alive_*, rtos_glue.h). CHI 1 noi duy nhat -
// loop() (than cua Idle task SAU vTaskStartScheduler(), xem main.cpp) - duoc
// goi wdt_feed(), va CHI feed khi rtos_all_tasks_alive() tra ve true (ca 3
// task deu vua diem danh gan day). Nho vay, neu DUNG 1 task bi treo trong khi
// 2 task con lai van chay binh thuong, watchdog VAN phat hien duoc va reset
// MCU - khong chi bat duoc truong hop treo toan he thong.
//
// LUU Y QUAN TRONG khi debug qua ST-Link: IWDG MOT KHI DA wdt_init() THI
// KHONG THE TAT LAI (dac tinh phan cung cua dong STM32 nay, khac WWDG). Neu
// halt CPU o breakpoint lau hon thoi gian timeout, IWDG van tiep tuc dem va
// se RESET MCU ngay ca khi dang debug binh thuong (khong lien quan loi code)
// - day la hien tuong ST-Link/debug BINH THUONG voi IWDG, khong phai bug.
// =========================================================

// goi 1 LAN duy nhat trong setup(), CANG SOM CANG TOT (truoc vTaskStartScheduler()).
// Tu doc + xoa co RCC "reset boi IWDG" NGAY LUC NAY (truoc khi begin() co the
// lam thay doi trang thai), cache lai de wdt_boot_was_reset() doc lai bao
// nhieu lan cung duoc (vd tu CLI "wdt stat") ma khong can xoa co lai.
void wdt_init(uint32_t timeout_us);

void wdt_feed(); // "cho an" watchdog - CHI goi tu loop() (xem main.cpp), KHONG goi noi khac

// true neu LAN KHOI DONG NAY la do watchdog reset gay ra (chan doan: lan
// chay truoc bi treo that su) - gia tri CACHE tu luc wdt_init(), doc lai bao
// nhieu lan cung duoc (khong tu xoa/thay doi sau moi lan doc).
bool wdt_boot_was_reset();
