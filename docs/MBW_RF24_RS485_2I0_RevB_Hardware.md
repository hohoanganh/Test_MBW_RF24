# MBW-RF-00-24 (MBW RF24 RS485 2.0) – Tổng quan phần cứng (từ Schematic)

Trích từ `docs/Shematic_MBW_RF24_RS485_2I0_RevB.pdf` (project `MBW_RF24_RS485_2I0_RevB.PrjPcb`, rev **B**, 13/05/2026, 4 trang: Power / RS485 / NRF24L01 / MCU) và trang sản phẩm
[epcb.vn – Thiết bị chuyển đổi RS485 sang Wireless MBW-RF-00-24](https://epcb.vn/products/thiet-bi-chuyen-doi-rs485-sang-wireless-mbw-rf-00-24).

## Giới thiệu sản phẩm

**MBW-RF-00-24** là thiết bị chuyển đổi **RS485 ⇄ Wireless 2.4GHz**, nhận gói tin Modbus từ RS485 và phát broadcast
qua sóng không dây (và ngược lại). Hỗ trợ cả **Modbus RTU** và **Modbus ASCII**. Dùng tần số **2.4GHz** với các
kênh (Channel) lệch so với kênh WiFi để tránh nhiễu lẫn nhau. Hỗ trợ tới **64 mạng Modbus** khác nhau (64 Network ID),
cấu hình bằng **DIP switch**, không cần phần mềm cấu hình.

## Thông tin chung (theo schematic RevB)

| Mục | Giá trị |
|---|---|
| MCU | **STM32L151C8T6** (LQFP48, ARM Cortex-M3, 64KB Flash) – U2 |
| Thạch anh MCU | X1 **8 MHz** (HSE) |
| RF Transceiver | **nRF24L01P-R** (QFN20, U3) – SPI, 2.4GHz, thạch anh 16MHz (X3) |
| RF Front-End | **RFX2401C** (U7, QFN16) – PA/LNA cho nRF24L01, anten qua đầu nối **SMA** (J2) |
| RS485 | **1 cổng** THVD1420DR (U13, 5V), DE/RE điều khiển bằng 1 chân hướng; đệm tín hiệu 2× 74LVC1G125 (U5/U9); bảo vệ 3× TVS SMAJ12CA; điện trở đầu cuối 120R qua **solder bridge SB1 (mặc định hở, chỉ hàn nối khi board là đầu cuối bus RS485)** |
| Kết nối RS485 | **RJ11** (J1, R-RJ11R06P-A800) – A/B + PE |
| RTC | IC ngoài **PCF85063ATL** (U8) qua I2C + pin nuôi **CR1220** (BT1) + thạch anh 32.768kHz (X2) |
| NOR Flash | **W25Q128JVSIQ** (U4, 128Mbit = 16MB) trên **SPI1**, dùng chung bus với nRF24L01, CS riêng (PB14) |
| Cấu hình | DIP switch 8 bit (S1) đọc qua shift register **74HC165D** (U10) |
| Buzzer | Có (LS1, MLT-8530), điều khiển qua transistor Q1 |
| LED | LED trạng thái 2 màu (D12, LED_LIFE) + LED báo TX/RX RS485 2 màu (D3, nối trực tiếp theo tín hiệu UART) |
| Nút bấm | Nút nhấn người dùng S2 (trên chân PA11, không phải NRST cứng) |
| Nạp/Debug | Header SWD 7 chân (J9): SWDIO/SWCLK + USART1 (console) |
| Mở rộng | Header 30 chân 2.54mm (J6) – xuất hầu hết chân MCU + nguồn 3V3/5V/VIN |
| Nguồn vào | **8 ~ 28 VDC** qua giắc vít J3 (DB2ERM-3.81-2P), có diode chống ngược D1 (DSK34) + TVS D2 (SMAJ28CA) |
| Nguồn trong | Buck **TPS54331DR** (U6) → **5V @1A**; LDO **RT9013-33GB** (U1) → **3.3V @300mA** |
| Kích thước | **70 × 62 mm** |

> Kiến trúc khác hẳn board OPMS/Smart PDU: **không có quạt, relay AC, cảm biến NTC/độ ẩm, GPI/GPO 48V**.
> Đây là board truyền thông RS485 ⇄ RF đơn giản, tập trung vào MCU + RF transceiver + RS485 transceiver.

## Các khối chức năng (theo trang schematic)

| Trang | Khối | Mô tả |
|---|---|---|
| 1 | Power | DC IN 8~28V (J3) → chống ngược D1 + TVS D2 → Buck TPS54331 → 5V → LDO RT9013 → 3.3V |
| 2 | RS485 | THVD1420 (USART2, DE/RE = PA1), đệm 74LVC1G125 ×2, RJ11 J1, TVS bảo vệ, DIP switch ID (74HC165), LED chỉ thị TX/RX |
| 3 | NRF24L01 | nRF24L01P-R (SPI1) + RF front-end RFX2401C (PA/LNA) + anten SMA |
| 4 | MCU | STM32L151C8T6, thạch anh 8MHz, NOR Flash W25Q128 (SPI1 dùng chung), RTC PCF85063 (I2C, pin CR1220), Buzzer, LED, nút nhấn, header debug, header mở rộng J6 |

## Bản đồ chân MCU (STM32L151C8T6, LQFP48) – trích từ netlist

### RF (nRF24L01, SPI1 dùng chung với Flash)

| Net | Chân | Chức năng |
|---|---|---|
| PA5/SCK1 | PA5 | SPI1 SCK – dùng chung cho nRF24L01 và W25Q128 |
| PA6/MISO1 | PA6 | SPI1 MISO – dùng chung |
| PA7/MOSI1 | PA7 | SPI1 MOSI – dùng chung |
| PA8/RF_CE | PA8 | Chip-Enable nRF24L01 |
| PB9/RF_CSN | PB9 | Chip-Select (SPI) nRF24L01 |
| PB1/RF_IRQ | PB1 | Ngắt (IRQ) nRF24L01 |
| PB14/W25CS | PB14 | Chip-Select NOR Flash W25Q128 |

### RS485 (USART2)

| Net | Chân | Chức năng |
|---|---|---|
| PA2/TX2 | PA2 | USART2 TX → DI (THVD1420) |
| PA3/RX2 | PA3 | USART2 RX ← RO (THVD1420) |
| PA1/RS_DIR | PA1 | Điều khiển DE/RE (hướng truyền) THVD1420 |

### DIP Switch ID (74HC165 shift register)

| Net | Chân | Chức năng |
|---|---|---|
| PC13/SW_LOAD | PC13 | SH/LD – chốt dữ liệu 74HC165 |
| PC14/SW_SCK | PC14 | CLK – xung clock dịch |
| PC15/SW_MISO | PC15 | QH – dữ liệu nối tiếp (8 bit SW1‑SW8) về MCU |

### RTC & NOR Flash (I2C / SPI)

| Net | Chân | Chức năng |
|---|---|---|
| PB6/I2C_SCL | PB6 | I2C SCL – RTC PCF85063 |
| PB7/I2C_SDA | PB7 | I2C SDA – RTC PCF85063 |
| PA5/PA6/PA7 | PA5/PA6/PA7 | SPI1 – NOR Flash W25Q128 (chung bus RF) |
| PB14/W25CS | PB14 | CS Flash |

### LED, Buzzer, nút nhấn

| Net | Chân | Chức năng |
|---|---|---|
| PB0/BUZ | PB0 | Điều khiển Buzzer (qua Q1) |
| PB8/LED_LIFE | PB8 | LED trạng thái 2 màu D12 (qua Q4) |
| PA11/BT_RST | PA11 | Nút nhấn người dùng S2 (kéo lên R28 10k) |
| MCU_RST (NRST) | NRST (chân 7) | Reset cứng MCU (R33/C44), đưa ra J6 |

### Console / Debug / Nạp

| Net | Chân | Chức năng |
|---|---|---|
| PA9/TX1 | PA9 | USART1 TX (console, ra header debug J9) |
| PA10/RX1 | PA10 | USART1 RX (console, ra header debug J9) |
| PA13/SWDIO | PA13 | SWD Debug |
| PA14/SWCLK | PA14 | SWD Clock |
| BOOT0 | chân 44 | Chọn chế độ boot (kéo qua R16) |

### Header mở rộng J6 (30 chân, 2.54mm) & Debug J9 (7 chân)

J6 xuất ra: +3V3, +5V, +VIN, MCU_RST, PA4/ADC, PB6/I2C_SCL, PB7/I2C_SDA, PB2/BOOT1, PA5/SCK1, PA6/MISO1,
PA7/MOSI1, PA9/TX1, PA10/RX1, PB12, PB13, PA3/RX2, PA1/RS_DIR, PA2/TX2, PB10/TX3, PB11/RX3, PB3, PB4, PB5.

J9 (Debug): +3V3, PA14/SWCLK, PA13/SWDIO, PA9/TX1, PA10/RX1, GND — gộp chung cổng nạp SWD và console UART1.

> PA12, PA15, PB3/JTDO, PB4/JNTRST, PB5, PB12, PB13, PB15, PC0‑PC12 không dùng cho chức năng cố định trên
> board này (một số được đưa ra J6 làm GPIO dự phòng). Nên đối chiếu lại trực tiếp trên schematic khi viết driver.

## Cấu hình DIP switch (S1, 8 bit) – theo tài liệu sản phẩm

| Switch | Chức năng |
|---|---|
| SW1 – SW6 | **Network ID** (64 tổ hợp = 64 mạng Modbus khác nhau) |
| SW7 – SW8 | **Baudrate** RS485: 4800 / 9600 / 14400 / 19200 |

> Bảng Channel tần số 2.4xx GHz trên trang sản phẩm áp dụng cho dòng sản phẩm MBW-RF nói chung; cần đối chiếu
> thêm với firmware thực tế để xác nhận cách chọn channel trên board RevB (không thấy switch/jumper riêng cho
> channel trong schematic – khả năng channel cố định trong firmware hoặc suy ra từ Network ID).

## Nạp firmware (ST-LINK / SWD)

SWD qua PA13 (SWDIO) / PA14 (SWCLK) tại header J9. Console UART1 (PA9/PA10) dùng chung header J9 để log/debug.
