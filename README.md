# MBW RF24 RS485 2.0 (MBW-RF-00-24) – Firmware & Test App

Firmware PlatformIO (Arduino/STM32duino, STM32L151C8T6) + app test Python (tkinter)
cho board **MBW RF24 RS485 2.0** (`MBW_RF24_RS485_2I0_RevB`). Sản phẩm: chuyển đổi
RS485 (Modbus RTU/ASCII) ⇄ Wireless 2.4GHz (nRF24L01P + RFX2401C PA/LNA), hỗ trợ 64
mạng Modbus độc lập (Network ID) chọn bằng DIP switch.

Tham khảo kiến trúc từ 2 project cũ trong `_reference/` (Smart PDU 2I0, OPMS 1.6
Slave): driver tách theo nhóm chức năng trong `src/drivers/`, CLI qua Console
UART1 115200, app test Python tkinter dùng chung theme, xuất báo cáo `.xlsx`.
Khác với 2 project tham khảo (Arduino-framework đơn luồng), firmware này (2026)
đã **chuyển sang FreeRTOS** (`stm32duino/STM32duino FreeRTOS`) với 3 task tối
giản để song song hóa thật RF/RS485/CLI — xem mục "Kiến trúc RTOS" bên dưới.

---

## Hai chức năng chính

1. **Chức năng RS485 không dây (bridge)** – chức năng MẶC ĐỊNH của sản phẩm:
   relay trong suốt dữ liệu RS485 ⇄ RF, **tự bật ngay khi cấp nguồn** (không
   cần lệnh CLI kích hoạt). Mỗi lần relay 1 khung, firmware in 1 dòng
   `FWD RS485->RF: ...` / `FWD RF->RS485: ...` ra console để quan sát trực
   tiếp hoạt động forward.
2. **App test (factory test)** – CLI qua Console (`id/ver/help/dip/rs485/rsl/
   flash/rtc/rf id/bridge...`) + app Python `mbw_test_app.py`. Vì 2 board khi
   truyền/nhận thật sẽ cắm vào **2 máy tính khác nhau**, app chỉ kết nối
   **1 board / 1 máy tính** — không điều khiển đồng thời 2 board. Mỗi máy tự
   test board của mình và quan sát hoạt động forward qua tab **"Giám sát
   Forward"** (đọc log `FWD ...` do board tự in ra).

---

## Cấu trúc thư mục

```
Test_MBW_RF24/                     (thư mục project)
├── README.md                      ← file này
├── platformio.ini                 ← build firmware (env: mbw_rf24)
├── boards/genericSTM32L151C8.json ← board tự định nghĩa (STM32L151C8T6, 64K/10K)
├── build_mbw_exe.bat              ← build app Python -> dist/MBW_RF24_Test.exe
│
├── mbw_test_app.py                ← APP test (tkinter), 1 board / 1 máy tính
├── modbus_poll_app.py             ← công cụ Modbus RTU Master thật (poll qua RS485 vật lý)
├── mbw_theme.py                   ← theme dùng chung (copy từ opms_theme.py)
├── app_config.json                ← kế hoạch test tự động (6 bước, 1 board)
│
├── src/                           ← firmware C++ (PlatformIO + Arduino + FreeRTOS)
│   ├── main.cpp                   ← setup(): tạo 3 task RTOS + vTaskStartScheduler()
│   ├── hal.h / hal.cpp            ← Console UART1, dispatch lệnh CLI (chạy trong task CLI)
│   ├── mbw_drv.h / mbw_drv.cpp    ← umbrella 7 driver + drv_init()
│   ├── mbw_common.h               ← PIN MAP + DEVICE_ID + baudrate
│   ├── rtos_glue.h / rtos_glue.cpp ← queue (g_qToRF/g_qToRS485) + mutex (g_muSPI/g_muSerial)
│   │                                 dùng chung giữa 3 task - xem "Kiến trúc RTOS"
│   └── drivers/
│       ├── drv_common.h/.cpp      ← khung tin RF 32 byte + CRC16-MODBUS dùng chung
│       ├── dipsw.cpp/.h           ← DIP switch 8-bit (74HC165): Network ID + Baud
│       ├── rs485.cpp/.h           ← RS485 USART2 (send/loopback/baud runtime)
│       ├── flashmem.cpp/.h        ← SPI Flash W25Q128 (id/read/write/erase)
│       ├── rtc.cpp/.h             ← RTC PCF85063 (I2C)
│       ├── ledbuzz.cpp/.h         ← LED_LIFE, Buzzer, nút nhấn S2
│       ├── rf_link.cpp/.h         ← nRF24L01+RFX2401C (thư viện RF24 + framing riêng)
│       └── bridge.cpp/.h          ← CHỨC NĂNG CHÍNH: cầu RS485 <-> Wireless
│
└── docs/
    └── MBW_Test_Procedure.md      ← hướng dẫn test 1 board/máy + quan sát forward
```

---

## Firmware (PlatformIO + Arduino/STM32duino)

Một firmware duy nhất — chức năng cầu RS485⇄Wireless **mặc định BẬT ngay khi
cấp nguồn** (không cần lệnh kích hoạt, đây là hành vi chuẩn của sản phẩm khi
lắp đặt thật). Lệnh CLI `bridge on|off` chỉ dùng để kỹ thuật viên tạm dừng khi
cần chạy các lệnh test RS485/RF thủ công khác.

### Build & nạp

```
Build  →  pio run
Nạp    →  pio run -t upload      (ST-Link, connect-under-reset)
Monitor→  pio device monitor --baud 115200
```

`platformio.ini`: board `genericSTM32L151C8` (STM32L151C8T6, 64K Flash/10K RAM),
`lib_deps = nrf24/RF24` (thư viện RF24 quản lý tầng SPI/protocol nRF24L01; owner
trên PlatformIO Registry là `nrf24`, không phải `tmrh20` — dự án đã chuyển tổ
chức GitHub, dùng `tmrh20/RF24` sẽ báo lỗi `UnknownPackageError`) và
`stm32duino/STM32duino FreeRTOS` (RTOS - xem mục dưới).

### Kiến trúc RTOS (3 task tối giản)

Chip chỉ có **10KB RAM** nên chuyển sang RTOS theo hướng **tối giản, ít task
nhất có thể** thay vì 1 task/driver — mục tiêu duy nhất là **song song hóa
thật** RF + RS485 + CLI (trước đây là 1 vòng `loop()` xử lý tuần tự, RS485 có
thể bị trễ nếu RF đang bận gửi lặp lại nhiều lần).

| Task | Ưu tiên | Việc chính |
|---|---|---|
| **RS485/Bridge** | Cao nhất (`RTOS_PRIO_RS485`) | Đọc RS485, phát hiện khoảng lặng 3.5 ký tự để tách khung Modbus — không được để task khác làm trễ, nếu không sẽ ghép sai khung. |
| **RF** | Trung bình (`RTOS_PRIO_RF`) | `rf_process()` (nhận + heartbeat) và `rf_send()` thật (có thể mất vài mili-giây do gửi lặp lại) — tách khỏi RS485 để không ảnh hưởng thời gian tách khung. |
| **CLI + ngoại vi** | Thấp nhất (`RTOS_PRIO_CLI`) | Console CLI, DIP, nút nhấn, buzzer, LED heartbeat — ít nhạy cảm thời gian nhất. |

**2 hàng đợi** (`g_qToRF`, `g_qToRS485`, độ sâu = 1) thay cho gọi hàm trực tiếp
giữa task RS485 và task RF — nhờ vậy task RS485 không bao giờ bị khóa cùng lúc
RF đang gửi lặp lại. Dùng `xQueueSend`/`xQueueReceive` **không chờ** (timeout=0):
hàng đợi đầy thì khung bị rớt (đếm ở `bridge stat`: `DROP_RS485_TO_RF`/
`DROP_RF_TO_RS485`) — chấp nhận được vì Modbus Master đã có cơ chế
timeout/retry riêng ở lớp trên.

**2 mutex** bảo vệ tài nguyên phần cứng dùng chung:
- `g_muSPI` — bus SPI1 dùng chung bởi nRF24L01 (`rf_link.cpp`) và Flash
  W25Q128 (`flashmem.cpp`). Trước đây an toàn vì `loop()` đơn luồng; nay bắt
  buộc phải khóa vì 2 driver có thể chạy trên 2 task khác nhau cùng lúc.
- `g_muSerial` — console `SerialDBG` dùng chung bởi cả 3 task (qua hàm
  `dbg_lock()`/`dbg_unlock()`), tránh xen ký tự giữa các dòng in của 2 task
  khác nhau (`FWD ...`, `RF LINK ...`, phản hồi lệnh CLI).

**Quy tắc thứ tự khóa (tránh deadlock):** nếu một đoạn code cần cả 2 mutex,
LUÔN khóa `g_muSPI` trước rồi mới `g_muSerial` (không bao giờ ngược lại). Cụ
thể: mọi lệnh CLI trong `hal.cpp` phải gọi xong các hàm `rf_*`/`flash_*` (tự
khóa/mở `g_muSPI` riêng) để lấy kết quả **trước**, rồi mới `dbg_lock()` để in —
không được giữ `g_muSerial` trong lúc gọi các hàm đó.

**RAM budget**: `configTOTAL_HEAP_SIZE` mặc định của thư viện là 15KB — vượt
quá cả 10KB RAM của chip, đã ghi đè xuống 4KB qua `build_flags` trong
`platformio.ini`. Stack từng task (`RTOS_STACK_RS485/RF/CLI`, đơn vị **word**
= 4 byte) khai báo trong `rtos_glue.h`. Dùng lệnh CLI **`rtos stat`** để xem
`xPortGetFreeHeapSize()` + stack còn trống từng task
(`uxTaskGetStackHighWaterMark()`) **trên phần cứng thật** — nếu báo thiếu RAM
lúc tạo task hoặc `vTaskStartScheduler()` không bao giờ chạy tới dòng lệnh sau
nó, giảm `RTOS_STACK_xxx` trước khi tăng `configTOTAL_HEAP_SIZE`.

> **Lưu ý xác thực:** code đã được kiểm tra cú pháp qua `g++ -fsyntax-only` với
> stub API FreeRTOS (không phải toolchain STM32 thật, vì sandbox không cài
> được PlatformIO+ARM toolchain). Việc build/nạp thật (`pio run`) và xác nhận
> RAM đủ dùng qua `rtos stat` trên board thật **vẫn cần thực hiện** trước khi
> coi là hoàn tất.

### Vì sao dùng thư viện RF24 thay vì tự viết driver SPI

Yêu cầu là liên kết RF **tối ưu, chịu nhiễu tốt trong môi trường công nghiệp**.
RF24 (TMRh20) đã được kiểm chứng rộng rãi ở tầng SPI/ShockBurst — tự viết lại
tầng này chỉ tăng rủi ro bug tinh vi (timing SPI, trạng thái FIFO...) mà không
cải thiện độ tin cậy. Phần **thực sự cần tùy biến cho môi trường nhiễu** được
làm ở lớp ứng dụng (`rf_link.cpp`), phía trên RF24:

| Lựa chọn | Lý do |
|---|---|
| `RF24_250KBPS` | Tốc độ thấp nhất = độ nhạy thu cao nhất, chịu nhiễu tốt nhất. RS485 4800–19200 baud vốn đã chậm hơn nhiều, không cần tốc độ RF cao. |
| `RF24_PA_MAX` + RFX2401C | PA/LNA ngoài khuếch đại thêm tầm phát/thu (TXEN/RXEN đấu cứng theo CE trên schematic, không cần GPIO riêng). |
| `RF24_CRC_16` (phần cứng) + CRC16-MODBUS (phần mềm) | 2 lớp kiểm tra toàn vẹn — loại gói lỗi mà nRF24 tự CRC có thể bỏ sót khi nhiễu mạnh. |
| Payload **cố định 32 byte** (không dynamic payload) | Bớt một lớp "bắt tay" giữa 2 đầu — ít điểm có thể lỗi khi tín hiệu yếu. |
| **Tắt auto-ack/auto-retry phần cứng**, tự gửi lặp lại (mặc định 3 lần, **tự động 2-6 lần**) + lọc trùng theo `(src_id, seq)` | Đây là kênh **broadcast** (nhiều thiết bị cùng Network ID cùng nghe 1 địa chỉ) — ACK 1-1 không hợp lệ khi có nhiều bên nhận. Gửi lặp lại không cần ACK vẫn tăng xác suất tới nơi trong môi trường nhiễu. |
| **Heartbeat + đo chất lượng link** (lấy ý tưởng từ bộ telemetry MAVLink/SiK radio) | Mỗi board tự phát 1 khung điều khiển nhỏ mỗi giây (không đụng vào dữ liệu Modbus) để bên kia biết "còn sống"; nếu quá 3s không nhận được gì (kể cả dữ liệu thật) thì báo `RF LINK: DOWN`. Tỷ lệ "kỳ mất trắng" đo được dùng để **tự tăng số lần gửi lặp khi link kém, tự giảm khi link ổn định trở lại** — tối ưu độ tin cậy mà không tốn băng thông khi không cần thiết. |

### Heartbeat + giám sát chất lượng link RF

Board tự gửi 1 khung điều khiển "heartbeat" mỗi 1 giây (`RF_HB_PERIOD_MS`), tách
biệt hoàn toàn với dữ liệu Modbus thật (dùng giá trị `frag_idx=0xFF` làm dấu
hiệu nhận biết, không thể trùng với khung dữ liệu thật). Cơ chế:

- **Bất kỳ khung hợp lệ nào** (dữ liệu thật hay heartbeat) đều tính là "còn nghe
  thấy nhau" — không cần đợi đúng heartbeat mới coi là link sống.
- Quá `RF_LINK_TIMEOUT_MS` (3 giây, ~3 chu kỳ heartbeat) không nhận được gì →
  in `RF LINK: DOWN (peer=<id>, <n>ms...)`. Khi nhận lại được → in
  `RF LINK: UP (peer=<id>)`. Giống cách Mission Planner báo "mất kết nối" khi
  hết heartbeat MAVLink.
- **Redundant TX tự thích ứng**: cứ mỗi chu kỳ 1 giây không nhận được gì tính
  là 1 "kỳ mất trắng" — 2 kỳ mất liên tiếp thì tự tăng số lần gửi lặp (tối đa
  6 lần); 5 kỳ tốt liên tiếp thì tự giảm về mức thấp hơn (tối thiểu 2 lần) để
  đỡ chiếm kênh khi link đã ổn định. In log `RF: link kém, tăng độ dự phòng
  lên N` / `RF: link ổn định, giảm độ dự phòng còn N` mỗi lần đổi.
- Lệnh `rf stat` hiển thị đầy đủ: `RF_LINK=UP|DOWN PEER=<id> AGE_MS=<n>
  LOSS_PROMILLE=<n> REDUND=<n> HB_TX=<n> HB_RX=<n>`.
- App test (`mbw_test_app.py`, tab Giám sát Forward) có nhãn **RF Link** hiển
  thị UP/DOWN theo thời gian thực (đọc dòng log tức thời) + nút "Đọc RF Link
  (rf stat)" để xem % mất và độ dự phòng hiện tại — tự động làm mới mỗi 3 giây
  khi đang kết nối.

### Lệnh CLI (Console USART1, 115200)

| Lệnh | Chức năng |
|---|---|
| `id` / `ver` / `help` | ID thiết bị / phiên bản FW / danh sách lệnh |
| `led` / `beep on\|off` | Toggle LED_LIFE / tắt-bật tiếng bíp |
| `dip` | Đọc DIP switch: `NETID` (0-63) + `BAUD` (4800/9600/14400/19200) |
| `rs485 <text>` | Gửi thử 1 chuỗi ra RS485 |
| `rsl` | Loopback RS485 (cần nối tắt A-B) |
| `rs485mon on\|off` | Forward RX RS485 ra console (KHÔNG bật cùng lúc với `bridge on`) |
| `baud rs485 <bps>` | Đổi baudrate RS485 lúc chạy |
| `flash` / `fwr` | Đọc JEDEC ID / test ghi-đọc SPI Flash W25Q128 |
| `rtc` / `rtc set <hh:mm:ss>` | Đọc/đặt giờ RTC PCF85063 |
| `rf id` | Kiểm tra nRF24L01 có mặt (SPI) |
| `rf ch <0-125>` | Đặt kênh RF (mặc định 120, vùng tránh WiFi 1-11) |
| `rf netid <0-63>` | Ghi đè Network ID lúc chạy (mặc định lấy từ DIP lúc boot) |
| `rf tx <text>` | Gửi 1 bản tin không dây thủ công (kỹ thuật viên tự kiểm tra RF thô, cần `bridge off` tạm thời) |
| `rf stat` / `rf reset` | Thống kê TX/RX OK/trùng/lỗi CRC/rớt mảnh + kênh/NETID + **LINK UP/DOWN, % mất, độ dự phòng tự thích ứng** |
| `bridge on\|off` | Tạm BẬT/TẮT chức năng cầu RS485⇄Wireless (**mặc định ON ngay khi cấp nguồn**) |
| `bridge log on\|off` | Bật/tắt in dòng `FWD ...` khi relay (**mặc định ON**, app đọc dòng này để hiển thị) |
| `bridge stat` / `bridge reset` | Thống kê số khung đã relay 2 chiều + trạng thái bridge/log + số khung bị rớt do hàng đợi liên-task đầy (`DROP_RS485_TO_RF`/`DROP_RF_TO_RS485`) |
| `rtos stat` | RAM heap còn trống (`xPortGetFreeHeapSize`) + stack còn trống từng task RS485/RF/CLI — kiểm tra ngân sách RAM RTOS trên phần cứng thật |

Khi `bridge off`, mọi bản tin RF nhận được sẽ tự in ra console dạng
`RF RX: <nội dung>` — dùng để kiểm tra RF thô giữa 2 board mà không qua bridge.

---

## App test (`mbw_test_app.py`, Python/tkinter)

Mỗi cửa sổ app chỉ kết nối **1 board qua 1 cổng COM** (vì 2 board sẽ cắm vào
2 máy tính khác nhau khi vận hành thật). Xem chi tiết quy trình test 2 máy
song song trong [`docs/MBW_Test_Procedure.md`](docs/MBW_Test_Procedure.md).

```bash
pip install pyserial openpyxl matplotlib
python mbw_test_app.py
```

3 tab chính:
- **Giám sát Forward** – tab mặc định: bảng log trực quan mọi khung đã relay
  (giờ, hướng `RS485→RF`/`RF→RS485`, số byte, preview hex) đọc từ dòng `FWD ...`
  board tự in ra; nút bật/tắt bridge & log, ô gửi thử `rs485 <text>` để tự kiểm
  tra forward khi chưa có Modbus Master/Slave thật. Nhãn **RF Link** hiển thị
  UP/DOWN theo heartbeat của board theo thời gian thực, nút "Đọc RF Link
  (rf stat)" xem chi tiết % mất và độ dự phòng đang tự thích ứng — tự làm mới
  mỗi 3 giây khi đang kết nối. Nút **"🔌 Modbus Poll Test
  (RS485 thật)"** mở `modbus_poll_app.py` trong cửa sổ/tiến trình riêng (giống
  cách `opms_test_app.py` mở `ch348_test_app.py`) — đây là **Modbus RTU Master
  thật** (FC3/FC6, log Excel, biểu đồ realtime) nối qua **cổng RS485 vật lý**
  của board (khác cổng console CLI ở trên) — phép test thực tế nhất cho chức
  năng bridge: máy A chạy Modbus Poll qua RS485 của board A, máy B nối board B
  vào 1 Modbus Slave thật (hoặc chạy Modbus Poll ở chiều ngược lại).
- **Test tự động** – 6 bước trên board đang nối (`app_config.json`): id → dip
  → flash → rtc → rsl (loopback, cần nối tắt A-B) → rf id.
- **Terminal** – gõ lệnh CLI tự do + nút lệnh nhanh.

**Đóng gói (.exe):** `build_mbw_exe.bat` → `dist\MBW_RF24_Test.exe` (PyInstaller
tự động gom `modbus_poll_app.py` vào chung file .exe vì được `import` trực
tiếp từ `mbw_test_app.py` khi chạy với cờ `--modbus`).

Báo cáo: `mbw_test_report.xlsx` (hoặc `.csv` nếu thiếu `openpyxl`) — mỗi máy
ghi report của board mình, nối tiếp mỗi lần chạy 1 hàng (không tạo file mới).
Log Modbus Poll (Excel riêng, do người dùng chọn nơi lưu) — xem `modbus_poll_app.py`.

---

## Cần xác nhận/hiệu chỉnh trên bàn test (calib)

- **Thứ tự bit DIP switch** (SW1-8 ↔ ngõ A-H của 74HC165): netlist trích xuất
  từ PDF bị nén, `dipsw.cpp` giả định bit0-5=SW1-6 (Network ID), bit6-7=SW7-8
  (Baudrate) — cần đối chiếu thực tế và sửa lại nếu khác.
- **TXEN/RXEN của RFX2401C**: tài liệu giả định đấu cứng theo PA8/RF_CE (không
  cần firmware can thiệp) — nên đo bằng oscilloscope lúc TX/RX để xác nhận.
  Nếu KHÔNG tự chuyển mạch theo CE, cần bổ sung GPIO điều khiển riêng trong
  `rf_link.cpp`.
- **Kênh RF mặc định (120)**: bảng kênh trên trang sản phẩm epcb.vn ghi
  2.520–2.525 (GHz) ứng với kênh nRF24 120-125 — chưa thấy switch/jumper chọn
  kênh riêng trên schematic, hiện cố định trong firmware (`rf ch` để đổi thủ
  công lúc test). Cần xác nhận cách chọn kênh thực tế của sản phẩm.
- **Log forward `FWD ...`** hiện in mọi khung relay — nếu lưu lượng Modbus cao
  khi lắp đặt thật, cân nhắc mặc định `bridge log off` để console đỡ dày đặc
  (app test vẫn dùng `bridge log on` khi cần quan sát).
- **`_reference/` chứa firmware/app các đợt trước** (đối chiếu pin map / luồng
  test cũ của OPMS & Smart PDU) — không phải code của board này.
- **RAM RTOS trên phần cứng thật**: `configTOTAL_HEAP_SIZE=4KB` + stack từng
  task (`rtos_glue.h`) mới chỉ được tính toán/kiểm tra cú pháp trong sandbox,
  CHƯA build/nạp thật lên STM32L151C8T6. Sau khi `pio run -t upload`, dùng lệnh
  `rtos stat` để xác nhận heap còn trống > 0 và stack từng task không về gần 0
  — nếu thiếu, giảm `RTOS_STACK_RS485/RF/CLI` trước khi tăng
  `configTOTAL_HEAP_SIZE`.
