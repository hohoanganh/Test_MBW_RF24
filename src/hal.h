#pragma once
#include <Arduino.h>

// hal.* = lop thap nhat: console UART1 (log + CLI), khoi tao chung (SPI/Wire),
// va bo dispatch lenh CLI (cli_execute goi vao cac ham cua driver trong
// src/drivers/* thong qua mbw_drv.h).

void hal_init();

void uart_log(const char *msg);

void cli_process(); // goi trong loop(): doc UART1, parse, thuc thi lenh
