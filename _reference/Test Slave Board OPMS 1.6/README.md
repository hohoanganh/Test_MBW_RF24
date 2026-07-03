# OPMS 1.6 Slave Board – Firmware Test & Test App

Bộ **test chức năng (functional test)** cho **OPMS 1.6 Slave Board**: firmware PlatformIO (C++,
FreeRTOS) nạp vào board để mở CLI qua cổng Console, kèm app test Python (tkinter) điều khiển /
giám sát qua COM và chấm PASS/FAIL theo ngưỡng.

- **MCU:** STM32F303VCT6 (LQFP100) · **Console:** RS232 USART1 (PA9/PA10), 115200 8-N-1
- **DEVICE_ID:** `OPMS_1.6_SLAVE` · **Firmware:** v0.3.2 (FreeRTOS)
- Pin map đầy đủ: `src/opms_common.h`, `docs/OPMS_1.6_PinMap.md`, `docs/OPMS_1.6_Hardware.md`

---

## Cấu trúc thư mục

```
Test Slave Board OPMS 1.6/
├── README.md                  ← tài liệu project (file này)
├── platformio.ini             ← cấu hình build firmware (1 env: opms_slave)
├── build_opms_exe.bat         ← build app Python → dist/OPMS_Slave_Test.exe
│
├── opms_test_app.py           ← APP test Slave Board (tkinter)
├── ch348_test_app.py          ← APP test mạch Console CH348 (6 cổng, loopback)
├── opms_theme.py              ← theme DÙNG CHUNG (màu/font/nút) cho cả 2 app
├── app_config.json            ← kế hoạch test (13 nhóm chức năng)
│
├── src/                       ← firmware C++ (PlatformIO + FreeRTOS)
│   ├── main.cpp               ← FreeRTOS: 3 task (Console / Heartbeat / Comm)
│   ├── hal.h / hal.cpp        ← Console UART, CLI parser, LED, buzzer
│   ├── opms_drv.h             ← umbrella: gộp 6 driver + drv_init()
│   ├── opms_drv.cpp           ← drv_init() (gọi *_init từng nhóm)
│   ├── opms_common.h          ← PIN MAP + DEVICE_ID + baudrate
│   └── drivers/               ← driver theo NHÓM chức năng
│       ├── drv_common.h       ← hằng ADC dùng chung
│       ├── io.cpp/.h          ← GPO, GPI (74HC165), điều khiển Relay AC
│       ├── fan.cpp/.h         ← Quạt 48V + Quạt nhỏ 12V + đọc rpm
│       ├── sensor.cpp/.h      ← NTC, độ ẩm + nguồn 5V cảm biến
│       ├── acmeter.cpp/.h     ← ĐỌC DÒNG AC qua CT (thư viện EmonLib)
│       ├── system.cpp/.h      ← Flash W25Q80 + version board (+ RTC sau)
│       └── comm.cpp/.h        ← RS485 (nguồn/cổng/debug) + UART Orange Pi
│
├── docs/                      ← tài liệu kỹ thuật
│   ├── HDSD_Test_OPMS_1.6_Slave.docx   ← hướng dẫn test cho KTV bàn test
│   ├── OPMS_1.6_Hardware.md / OPMS_1.6_PinMap.md
│   └── OPMS_1.6_Test_Procedure*.md / OPMS_App_TestFlow.md
└── _reference/                ← firmware/app/installer các đợt trước (.gitignore)
```

---

## Firmware (PlatformIO + FreeRTOS)

`main.cpp` chạy **FreeRTOS** với 3 task:

- **Console** — đọc UART console, parse CLI, thực thi lệnh (`cli_process`), giữ mutex UART khi in.
- **Heartbeat** — nháy LED LIFE 1Hz + buzzer; nhờ preempt nên **LED không ngưng nháy khi đang đo**.
- **Comm** — monitor Orange Pi (USART3) + RS485 ở nền, in RX ra console (mutex).

Driver được **tách theo nhóm** trong `src/drivers/` (io · fan · sensor · acmeter · system · comm);
`opms_drv.h` là umbrella gộp 6 header, `drv_init()` gọi `io_init/fan_init/…`. Đọc dòng AC dùng
**EmonLib** (CT + `calcIrms`) trong `acmeter.cpp`.

### Build & nạp

```
Build  →  pio run                 (lần đầu tự tải EmonLib + STM32FreeRTOS)
Nạp    →  pio run -t upload        (ST-Link, connect-under-reset)
Monitor→  pio device monitor       (Console USART1, 115200)
```

`platformio.ini`: board `disco_f303vc`, `-D ADC_BITS=12` (EmonLib dùng ADC 12-bit),
`lib_deps` = EmonLib + STM32FreeRTOS (git URL).

### Lệnh CLI (Console 115200)

| Lệnh | Chức năng | Trả về |
|---|---|---|
| `id` / `ver` / `bver` | ID / phiên bản FW / version board | `OPMS_1.6_SLAVE` · `FW 0.3.2` |
| `help` / `led` / `beep` | trợ giúp / LED RUN / còi | |
| `gpo <1-4> <on\|off>` / `gpoi` | Bật/tắt ngõ ra / đọc dòng tổng | `GPO_CUR=<mA>` (ACS712) |
| `fan <1-4> <on\|off> [pct]` / `fanr <1-4>` | Quạt 48V (kèm % tốc độ) / đọc rpm | `FANn_RPS=<rpm>` |
| `gpi` | Đọc 8 đầu vào (74HC165) | `GPI=<0-255>` |
| `ac <on\|off>` / `aci` | Relay AC / đọc dòng (EmonLib) | `AC_CUR=<mA>` |
| `dfan <1-3> <on\|off>` / `dfanfb` / `dfanr <1-2>` | Quạt nhỏ 12V / feedback / rpm | `DFANn_FB` · `DFANn_RPS` |
| `flash` | Đọc ID W25Q80 | `FLASH=OK/FAIL` (0xEF Winbond) |
| `ntc <1-4>` / `hum <1-2>` | Đọc nhiệt độ / độ ẩm | `0–255` |
| `humpwr <1-2> <on\|off>` | Nguồn 5V cảm biến độ ẩm | `OK HUMn=...` |
| `rspwr <on\|off>` / `rspg` | Nguồn 12V RS485 / power-good | `RS485_PG=<0/1>` |
| `rs485 <1-4> [text]` | Probe cổng RS485 (đếm byte nhận) | `RS485n_RX=<count>` |
| `rs485mon` / `rs485tx` / `rs485dir` | Debug RS485 (nghe / phát / giữ DIR), `q`=thoát | |
| `pi` | Loopback UART Orange Pi (USART3) | `PI=OK/FAIL` *(scaffold)* |

> Firmware v0.3.2 đã **bỏ lệnh `rtc`/`life`** (LED LIFE quan sát bằng mắt). Bước "Giao tiếp
> Orange Pi" hiện là **khung chờ** (loopback chập TX-RX), chốt cách đấu thật sau.

---

## App test (Python / tkinter)

Hai app, phong cách **Industrial/Engineering** (nền sáng, card trắng, primary xanh `#295C9A`),
dùng chung `opms_theme.py`:

- **`opms_test_app.py`** — test Slave Board. Kết nối COM (xác thực `OPMS_1.6_SLAVE`), nút
  **► Run test** chạy tự động 13 nhóm, **⚙ Setup** chỉnh ngưỡng PASS, **Terminal** xem/gõ lệnh,
  bảng kết quả (PASS xanh / FAIL đỏ / chạy xanh dương / chờ xám) + thanh tiến độ. Xuất report
  **nối tiếp mỗi board 1 hàng** vào `test_report.xlsx`.
- **`ch348_test_app.py`** — test mạch Console **CH348** 6 cổng bằng **loopback** (chập TX-RX
  từng cổng). Tự dò cổng theo VID 1A86, ghi `console_report.xlsx`.

```bash
pip install pyserial openpyxl pillow
python opms_test_app.py          # app chính
```

Trong app chính bấm **🔌 Test Console CH348** để mở cửa sổ CH348 (chạy lại chính chương trình
với cờ `--console` → tiến trình/cửa sổ riêng).

**Đóng gói (1 file .exe):** chạy `build_opms_exe.bat` → `dist\OPMS_Slave_Test.exe`
(nhúng sẵn `opms_theme` + `ch348_test_app`; nút Test Console gọi `…exe --console`).

### Kế hoạch test – `app_config.json`

13 nhóm: Nhiệt độ, Nguồn 5V độ ẩm, Độ ẩm, Đầu vào (I1–I8), Đầu ra (O1–O4), Quạt 48V, Relay AC,
Quạt nhỏ 12V, Nguồn 12V RS485, Cổng RS485, Flash, Feedback, **Giao tiếp Orange Pi**.
Mỗi nhóm gồm `name`, `list_test[]` với `des` / `handle {control, port, all}` / `thres`.

### Ngưỡng PASS (nút ⚙ Setup — chỉnh được, lưu dùng ngay)

| Mục | Min–Max | Đơn vị |
|---|---|---|
| Đầu ra (GPO, khi bật) | 80 – 320 | mA |
| Quạt 48V (khi bật) | 1800 – 3900 | rpm (định mức ~3500) |
| Quạt nhỏ 12V (khi bật) | 800 – 6000 | rpm |
| Relay AC (khi đóng) | 140 – 260 | mA |
| Nhiệt độ NTC (cắm) | 25 – 29 | giá trị NTC |
| Độ ẩm (cắm) | 45 – 65 | % |

---

## Cần hiệu chỉnh trên bàn test (calib)

- Hệ số dòng GPO (ACS712) và **AC_ICAL** của EmonLib (theo tỉ số CT / điện trở burden).
- Số xung FG mỗi vòng quạt 48V & quạt nhỏ (ra rpm đúng).
- Thứ tự bit GPI; thang đo NTC / độ ẩm (map ADC → dải 25–29 / 45–65).
- Ánh xạ cổng RS485 1–4 ↔ transceiver / chân DIR.
- Cách đấu test **giao tiếp Orange Pi** (loopback / Pi echo) — đang để khung chờ.

## Ghi chú

- Baud Console: **115200**. Báo cáo: `test_report.xlsx` (slave) và `console_report.xlsx` (CH348),
  mỗi thiết bị 1 hàng, ghi nối tiếp (không tạo file mới).
- `_reference/` chứa firmware `.bin` + app/installer các đợt trước (đối chiếu pin map / luồng test cũ);
  thư mục này được `.gitignore`.
