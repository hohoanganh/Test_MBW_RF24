# OPMS 1.6 Slave Board – Pin Map & Chức năng

> Điền từ schematic (`_reference/Schematic_OPMS Slave Board 1.6.pdf`).
> Chi tiết hơn xem `docs/OPMS_1.6_Hardware.md`. Định nghĩa pin đã đưa sang `src/opms_common.h`.

## Thông tin chung

| Mục                  | Giá trị                                  |
| -------------------- | ---------------------------------------- |
| MCU / Board          | **STM32F303VCT6** (LQFP100, U17)          |
| Clock                | HSE 8 MHz (Y1), RTC 32.768 kHz (Y2) + VBAT |
| Device ID (CLI "id") | `OPMS_1.6_SLAVE` _(xác nhận theo FW test)_ |
| Baud Console         | `115200` _(xác nhận)_                     |
| Console              | RS232 MAX3232 → **USART1** (PA9/PA10)      |
| SPI Flash            | **W25Q80** trên SPI3 (CS = PD7)           |
| RTC                  | RTC nội STM32 (không có IC ngoài)         |
| FW tham khảo         | `_reference/Firmware OPMS 1I6/`           |

## Pin map

| Khối | Chân MCU | Mức tích cực / Ghi chú |
| --- | --- | --- |
| LED LIFE (nhịp) | PE7 | Nạp xong nháy = OK |
| LED RUN | PE3 | Trạng thái chạy |
| Buzzer | PE0 | TMB12A05 active |
| Reset button | NRST (S1) | — |
| Console USART1 (TX/RX) | PA9 / PA10 | RS232 qua MAX3232 |
| RS485 nhóm A: USART2 (TX/RX) + DIR | PA2 / PA3 / PE6 | THVD1550 cổng A1,A2 |
| RS485 nhóm B: UART4 (TX/RX) + DIR | PC10 / PC11 / PA15 | THVD1550 cổng A3,A4 |
| RS485 power (EN/PG) | PC0 / PC1 | Nguồn 12V cổng RS485 |
| UART↔Orange Pi: USART3 (TX/RX) | PB10 / PB11 | Cầu PI_UART4 |
| SPI Flash (SCK/MISO/MOSI/CS) | PB3 / PB4 / PB5 / PD7 | W25Q80 |
| GPI 74HC165 (LOAD/EN/CLK/DATA) | PA11 / PA12 / PB6 / PB7 | 8 đầu vào 48V cách ly |
| GPO (O1–O4) | PD9 / PD10 / PD11 / PD12 | MOSFET, dòng: GPO_CUR PB13, offset PB12 |
| Quạt 48V PWM (1–4) | PC6 / PC7 / PC8 / PC9 | — |
| Quạt 48V TACH (1–4) | PD6 / PD5 / PD4 / PD3 | Đo tốc độ |
| Quạt nhỏ DEV FAN CTR (1–3) | PB8 / PC12 / PD1 | — |
| Quạt nhỏ DEV FAN FB (1–3) | PB9 / PD0 / PD2 | Feedback |
| Relay AC (ACC_CTR) | PE5 | G2RL; dòng AC: ADC_CT PB0 |
| NTC nhiệt độ (1–4) | PA5 / PA4 / PA6 / PA7 | ADC, NTC 10K |
| Độ ẩm DATA (1–2) | PC4 / PC5 | ADC |
| Độ ẩm EN 5V (1–2) | PE10 / PE15 | Cấp nguồn cảm biến |
| Version config (0–2) | PD13 / PD14 / PD15 | 011 = v1.6 |
| SWD (SWDIO/SWCLK) | PA13 / PA14 | Nạp/debug |

## Danh sách chức năng cần test

| # | Chức năng | Mô tả | Lệnh CLI dự kiến |
|---|---|---|---|
| 1 | Nhiệt độ | Đọc 4 NTC (ADC). Không cắm = 255; cắm = 25–29 | `temp` / `ntc` |
| 2 | Nguồn 5V độ ẩm | Bật/tắt EN cấp 5V cổng H1/H2, đo 5V/0V | `humpwr <1-2> <on/off>` |
| 3 | Độ ẩm | Đọc 2 cổng RHT. Không cắm = 255; cắm = 45–65 | `hum` |
| 4 | Đầu vào | Đọc 8 GPI (74HC165). Không cắm = 1; cắm = 0 | `gpi` |
| 5 | Đầu ra | Bật/tắt O1–O4, đo dòng (80–320 mA / 0) | `gpo <1-4> <on/off>` |
| 6 | Quạt 48V | Bật/tắt 1–4, đọc TACH (45–65 rps / 0) | `fan <1-4> <on/off>` |
| 7 | Relay AC | Bật/tắt, đo dòng (140–260 mA / 0–50) | `ac <on/off>` |
| 8 | Quạt nhỏ | Bật/tắt tất cả, đo dòng (50–150 mA / 0) | `dfan <on/off>` |
| 9 | Nguồn 12V RS485 | Bật/tắt, đo 12V/0V | `rspwr <on/off>` |
| 10 | Cổng RS485 | Đọc lần lượt A1B1→A4B4 (loopback/Modbus) | `rs485 <1-4>` |
| 11 | Flash & RTC | Đọc ID W25Q80 + RTC nội | `flash` / `rtc` |
| 12 | Feedback | Đọc trạng thái chân feedback | `fb` |

> Tên lệnh CLI ở trên là **dự kiến** — cần đối chiếu/định nghĩa theo firmware test OPMS 1.6 thực tế
> (giải nén `_reference/Firmware OPMS 1I6/opms_slave_app_test.rar` hoặc bắt log Console khi chạy).

## Lệnh CLI (Console)

| Lệnh | Chức năng |
|---|---|
| `id` | In Device ID để app xác thực |
| `ver` | Phiên bản firmware |
| `help` | Danh sách lệnh |
| _(bổ sung theo FW test)_ |  |
