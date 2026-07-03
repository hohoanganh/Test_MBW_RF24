# OPMS 1.6 Slave Board – Tổng quan phần cứng (từ Schematic)

Trích từ `_reference/Schematic_OPMS Slave Board 1.6.pdf` (rev 1.6, 02/2023, 14 trang).
Đây là cơ sở để viết firmware test + app test.

## Thông tin chung

| Mục | Giá trị |
|---|---|
| MCU | **STM32F303VCT6** (LQFP100) – U17 |
| Thạch anh | Y1 **8 MHz** (HSE), Y2 **32.768 kHz** (RTC) |
| RTC | **RTC nội** STM32F303 + pin nuôi **VBAT** (BT1) — *không có IC RTC ngoài* |
| SPI Flash | **W25Q80** (U18, 1 MB) trên **SPI3**, CS riêng |
| Console | **RS232** qua MAX3232 (U15) + RJ45 — UART **USART1** (PA9/PA10) |
| RS485 | **4 cổng** THVD1550 (×4), cách ly, level shift TXS0108 |
| Buzzer | **TMB12A05** (LS1) – active, net BUZZER |
| Board version | 3-bit **VER_CFG[2:0]** → `011 = v1.6` |
| Kích thước | 198 × 104 mm, 2 lớp, FR4 1.6mm |
| Nguồn vào | 48V (36–72V) hoặc 12V; buck TPS54331 → 5V; LM1117 → 3.3V |

> Lưu ý kiến trúc khác hẳn board Smart PDU: **không có PCA9554** (IO expander I2C),
> **không có IC RTC ngoài**. Đầu vào số dùng **shift register 74HC165**, ngõ ra/đầu vào
> đều **cách ly quang** (TLP293-4), đo dòng bằng **ACS712** + ADC nội.

## Các khối chức năng (theo trang schematic)

| Trang | Khối           | Mô tả                                                                              |
| ----- | -------------- | ---------------------------------------------------------------------------------- |
| 2     | Power          | 48V/12V → 5V (TPS54331 ×2) → 3.3V (LM1117). Nguồn 5V riêng cho Orange Pi.          |
| 3     | NTC & RHT      | 4 cổng nhiệt độ NTC 10K + 2 cổng độ ẩm/RHT (có chân EN cấp 5V). Opamp LM324/LM358. |
| 4     | Fan (48V)      | 4 quạt 48V: PWM ra + TACH (đo tốc độ) về, cách ly TLP293-4, MOSFET.                |
| 5     | GPI            | 8 đầu vào số 48V cách ly quang, đọc qua **74HC165** (shift register).              |
| 6     | GPO            | 4 ngõ ra MOSFET IRF540, cách ly TLP293-4, đo dòng tổng bằng **ACS712**.            |
| 7     | DEV FAN        | Quạt nhỏ thiết bị: 3 kênh control + feedback (12V_FAN).                            |
| 8     | ACC Controller | Relay AC **G2RL-2 12V** (Air Condition) + đo dòng AC (ADC_CT).                     |
| 9     | RS485          | 4× THVD1550 (USART2 + UART4), TXS0108 level shift, B0505S cách ly 5V.              |
| 10    | RS232          | MAX3232 + RJ45 kép — cổng Console (UART1).                                         |
| 11    | Orange Pi      | Header Tinker Board/Orange Pi: PI_UART1, PI_UART4, Ethernet RJ45.                  |
| 12    | MCU            | STM32F303VCT6, W25Q80 flash, buzzer, LED LIFE/RUN, version config.                 |

## Bản đồ chân MCU (STM32F303VCT6) – trích từ netlist

### Giao tiếp / Bus

| Net                    | Chân            | Chức năng                     |
| ---------------------- | --------------- | ----------------------------- |
| USART2_TX / RX         | PA2 / PA3       | RS485 nhóm A (cổng A1/A2)     |
| RS485_DIR1             | PE6             | Hướng truyền RS485 nhóm A     |
| UART4_TX / RX          | PC10 / PC11     | RS485 nhóm B (cổng A3/A4)     |
| RS485_DIR2             | PA15            | Hướng truyền RS485 nhóm B     |
| RS485_PWREN            | PC0             | Bật nguồn 12V cổng RS485      |
| RS485_PG               | PC1             | Power-good nguồn RS485        |
| USART1_TX / RX         | PA9 / PA10      | **Console RS232** (MAX3232)   |
| USART3_TX / RX         | PB10 / PB11     | Cầu sang Orange Pi (PI_UART4) |
| SPI3_SCK / MISO / MOSI | PB3 / PB4 / PB5 | SPI Flash W25Q80              |
| SPI_CS                 | PD7             | Chip-select W25Q80            |
| SWDIO / SWCLK          | PA13 / PA14     | Nạp/debug SWD                 |

### Cảm biến (ADC)

| Net                       | Chân                  | Chức năng                    |
| ------------------------- | --------------------- | ---------------------------- |
| NTC1 / NTC2 / NTC3 / NTC4 | PA5 / PA4 / PA6 / PA7 | 4 cổng nhiệt độ NTC 10K      |
| HUM1_DATA / HUM2_DATA     | PC4 / PC5             | 2 cổng dữ liệu độ ẩm/RHT     |
| HUM1_EN / HUM2_EN         | PE10 / PE15           | Cấp nguồn 5V cảm biến độ ẩm  |
| ADC_CT                    | PB0                   | Đo dòng relay AC             |
| GPO_CUR                   | PB13                  | Đo dòng tổng ngõ ra (ACS712) |
| GPO_OFFSET                | PB12                  | Offset/Vref đo dòng ngõ ra   |

### Quạt 48V

| Net | Chân | | Net | Chân |
|---|---|---|---|---|
| FAN1_PWM | PC6 | | FAN1_TACH | PD6 |
| FAN2_PWM | PC7 | | FAN2_TACH | PD5 |
| FAN3_PWM | PC8 | | FAN3_TACH | PD4 |
| FAN4_PWM | PC9 | | FAN4_TACH | PD3 |

### Đầu vào (GPI – 74HC165) & Đầu ra (GPO)

| Net | Chân | Chức năng |
|---|---|---|
| 74HC165_LOAD | PA11 | Chốt dữ liệu GPI |
| 74HC165_EN (CLK_INH) | PA12 | Cho phép clock GPI |
| 74HC165_CLK | PB6 | Clock dịch GPI |
| 74HC165_DATA | PB7 | Dữ liệu nối tiếp GPI về MCU |
| GPO1 / GPO2 / GPO3 / GPO4 | PD9 / PD10 / PD11 / PD12 | 4 ngõ ra MOSFET |

### Quạt nhỏ thiết bị (DEV FAN)

| Net | Chân | | Net | Chân |
|---|---|---|---|---|
| DEV_FAN1_CTR | PB8 | | DEV_FAN1_FB | PB9 |
| DEV_FAN2_CTR | PC12 | | DEV_FAN2_FB | PD0 |
| DEV_FAN3_CTR | PD1 | | DEV_FAN3_FB | PD2 |

### Điều khiển khác / Trạng thái

| Net | Chân | Chức năng |
|---|---|---|
| ACC_CTR | PE5 | Điều khiển relay AC (G2RL) |
| BUZZER | PE0 | Buzzer TMB12A05 |
| LED_LIFE | PE7 | LED nhịp (nạp xong nháy = OK) |
| LED_RUN | PE3 | LED trạng thái chạy |
| VER_CFG0 / 1 / 2 | PD13 / PD14 / PD15 | Chọn version board (011 = 1.6) |

> Các chân nguồn (VDD/VSS/VDDA/VREF/VBAT) và một số GPIO dự phòng không liệt kê ở đây.
> Một vài ánh xạ ADC (thứ tự NTC1↔PA5/PA4) nên đối chiếu lại trực tiếp trên schematic khi viết driver.

## Ánh xạ Khối phần cứng → Nhóm test (`app_config.json`)

| Nhóm test (app_config) | Khối phần cứng | Chân/đường liên quan |
|---|---|---|
| Nhiệt độ (T, 4) | NTC 10K ×4 | PA5/PA4/PA6/PA7 (ADC) |
| Nguồn 5V độ ẩm (PWR H, 2) | HUM_EN | PE10 / PE15 |
| Độ ẩm (H, 2) | RHT data | PC4 / PC5 (ADC) |
| Đầu vào (I, 8) | GPI 74HC165 (opto 48V) | PA11/PA12/PB6/PB7 |
| Đầu ra (O, 4) | GPO MOSFET + ACS712 | PD9–PD12, dòng: PB13 |
| Quạt (Fan, 4) | Quạt 48V PWM/TACH | PWM PC6–PC9, TACH PD6/PD5/PD4/PD3 |
| Relay AC (AC, 1) | Relay G2RL + đo dòng | ACC_CTR PE5, dòng ADC_CT PB0 |
| Quạt nhỏ (FC, 2) | DEV FAN | PB8/PB9, PC12/PD0, PD1/PD2 |
| Nguồn 12V RS485 (PWR, 1) | RS485 power | RS485_PWREN PC0, PG PC1 |
| Cổng RS485 (AB, 4) | THVD1550 ×4 | USART2 (A1/A2), UART4 (A3/A4) |
| Flash & RTC | W25Q80 + RTC nội | SPI3 PB3/PB4/PB5/CS PD7; RTC 32.768k |
| Feedback | Chân feedback | DEV_FANx_FB / trạng thái ngõ ra |

## Nạp firmware (ST-LINK)

| File | Địa chỉ |
|---|---|
| Boot test | `0x08000000` |
| App test | `0x08003000` |
| Deploy (chính thức) | APP `0x08003000` |

Nạp xong **đèn LIFE (LED_LIFE) nháy** = OK. SWD qua PA13/PA14.
