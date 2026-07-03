# MBW-RF-00-24 (MBW RF24 RS485 2.0) – Pin Map & Chức năng

> Điền từ schematic `docs/Shematic_MBW_RF24_RS485_2I0_RevB.pdf` (`MBW_RF24_RS485_2I0_RevB.PrjPcb`, rev B).
> Chi tiết khối chức năng xem `docs/MBW_RF24_RS485_2I0_RevB_Hardware.md`.

## Thông tin chung

| Mục | Giá trị |
|---|---|
| MCU / Board | **STM32L151C8T6** (LQFP48, U2) |
| Clock | HSE 8 MHz (X1); RTC ngoài PCF85063 dùng thạch anh riêng 32.768 kHz (X2) |
| RF | nRF24L01P-R (U3, SPI1) + RF front-end RFX2401C (U7) + anten SMA (J2) |
| RS485 | THVD1420DR (U13) trên USART2, DE/RE = PA1; kết nối qua RJ11 (J1) |
| SPI Flash | **W25Q128JVSIQ** (16MB) trên SPI1 (chung bus với nRF24L01), CS = PB14 |
| RTC | IC ngoài **PCF85063ATL** (I2C: PB6/PB7), pin nuôi CR1220 (BT1) |
| Console | USART1 (PA9/PA10) ra header debug J9 |
| Nguồn vào | 8~28 VDC qua J3 (screw terminal) |
| Board size | 70 × 62 mm |

## Pin map

| Khối | Chân MCU | Mức tích cực / Ghi chú |
|---|---|---|
| SPI1 SCK/MISO/MOSI (dùng chung RF + Flash) | PA5 / PA6 / PA7 | nRF24L01 + W25Q128 |
| RF CE / CSN / IRQ (nRF24L01) | PA8 / PB9 / PB1 | Điều khiển + ngắt RF |
| Flash CS (W25Q128) | PB14 | Chip-select riêng cho Flash |
| RS485 USART2 (TX/RX) + DIR | PA2 / PA3 / PA1 | THVD1420, DE/RE = PA1 |
| DIP Switch ID – 74HC165 (LOAD/CLK/DATA) | PC13 / PC14 / PC15 | 8 bit: SW1‑SW6 = Network ID, SW7‑SW8 = Baudrate |
| RTC I2C (SCL/SDA) | PB6 / PB7 | PCF85063ATL, pin CR1220 (BT1) |
| Buzzer | PB0 | Qua transistor Q1 (MMBT3904) |
| LED trạng thái (LED_LIFE, 2 màu) | PB8 | Qua transistor Q4 (MMBT3904) |
| LED báo TX/RX RS485 (2 màu, D3) | — (bám theo PA2/PA3) | Nối trực tiếp qua Q2/Q3 theo tín hiệu UART, không qua GPIO riêng |
| Nút nhấn người dùng (S2) | PA11 | Kéo lên R28 10k, không phải NRST |
| Reset cứng MCU (NRST) | NRST | R33/C44, đưa ra J6 (net MCU_RST) |
| Console USART1 (TX/RX) | PA9 / PA10 | Ra header debug J9 |
| SWD (SWDIO/SWCLK) | PA13 / PA14 | Nạp/debug, header J9 |
| BOOT0 | Chân 44 | Chọn chế độ boot |
| ADC dự phòng | PA4 | Đưa ra J6 (ADC), chưa gán chức năng cố định |
| Header mở rộng J6 (thêm) | PB2/BOOT1, PB3, PB4, PB5, PB10/TX3, PB11/RX3, PB12, PB13 | Đưa ra J6, dự phòng/USART3 |

## Danh sách chức năng cần test

| # | Chức năng | Mô tả | Ghi chú |
|---|---|---|---|
| 1 | Nguồn vào | Đo áp DC IN (8~28V), áp 5V (TPS54331), áp 3.3V (RT9013) | Kiểm tra chống ngược D1 + TVS D2 |
| 2 | RS485 | Loopback / Modbus qua RJ11 (J1), baudrate theo SW7‑8 (4800/9600/14400/19200) | DE/RE = PA1, USART2 |
| 3 | DIP Switch ID | Đọc 8 bit qua 74HC165 (SW1‑6 = Network ID 0‑63, SW7‑8 = Baudrate) | LOAD=PC13, CLK=PC14, DATA=PC15 |
| 4 | RF nRF24L01 | Kiểm tra SPI, phát/nhận gói tin 2.4GHz qua RFX2401C + anten SMA | CE=PA8, CSN=PB9, IRQ=PB1 |
| 5 | NOR Flash | Đọc ID/ghi-đọc W25Q128 (16MB) | CS=PB14, SPI1 chung bus RF – kiểm tra tranh chấp bus |
| 6 | RTC | Đọc/set thời gian PCF85063 qua I2C, kiểm tra pin nuôi CR1220 | I2C: PB6/PB7 |
| 7 | Buzzer | Bật/tắt buzzer qua PB0 | Qua Q1 |
| 8 | LED trạng thái | Bật/tắt LED 2 màu (LED_LIFE) qua PB8 | Qua Q4 |
| 9 | LED TX/RX RS485 | Quan sát LED D3 theo hoạt động UART | Không điều khiển trực tiếp qua GPIO |
| 10 | Nút nhấn | Đọc trạng thái nút S2 qua PA11 | Kéo lên 10k |
| 11 | Console/Debug | Log qua USART1 (J9), nạp SWD qua PA13/PA14 | Header J9 7 chân |

> Tên lệnh CLI, giá trị ngưỡng đo (dòng, áp) và cách chọn RF channel cần đối chiếu/định nghĩa theo firmware test
> thực tế của board MBW RF24 RS485 (chưa có trong schematic).

## Ghi chú khác biệt so với tài liệu cũ (OPMS_1.6)

Board này **không có**: quạt 48V, GPI/GPO 48V cách ly quang, cảm biến NTC/độ ẩm, relay AC, IO expander I2C,
Orange Pi header, RS232/MAX3232. Đây là board RS485 ⇄ RF 2.4GHz đơn giản (MCU + nRF24L01 + THVD1420),
kích thước 70×62mm, khác hoàn toàn kiến trúc board OPMS Slave/Smart PDU (198×104mm) mà file cũ mô tả nhầm.
