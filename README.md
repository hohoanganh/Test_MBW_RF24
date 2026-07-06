# MBW RF24 RS485 2.0 (MBW-RF-00-24) – Firmware & Test App

Firmware PlatformIO (Arduino/STM32duino, STM32L151C8T6) + app test Python (tkinter)
cho board **MBW RF24 RS485 2.0** (`MBW_RF24_RS485_2I0_RevB`). Sản phẩm: chuyển đổi
RS485 (Modbus RTU/ASCII) ⇄ Wireless 2.4GHz (nRF24L01P + RFX2401C PA/LNA).

Quy mô mạng: **1 Hub + tối đa 64 Slave dùng chung 1 NET_ID** (triển khai thực tế
trước mắt: 1 Hub + 16 Slave — xem
[`docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md`](docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md)).
2 con số độc lập nhau (2026-07-04/05, xem mục "Kiến trúc mạng: DEV_ID vs NET_ID"
bên dưới):
- **DEV_ID** (0-63, DIP switch SW1-6) — định danh RF **riêng từng board** trong
  1 mạng: `0` = Hub, `1-63` = Slave. Độc lập hoàn toàn với địa chỉ Modbus thật.
- **NET_ID** (0-63, CLI `net id` + lưu Flash) — hằng số **chung cho cả 1
  deployment** (Hub + mọi Slave cùng giá trị), dùng để cách ly nhiều lắp đặt
  RS485 độc lập nằm gần nhau về vật lý (vùng phủ RF chồng lấn).

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
│       ├── drv_common.h/.cpp      ← khung tin RF 32 byte (dev_id/seq/frag/len/CRC16) dùng chung
│       ├── dipsw.cpp/.h           ← DIP switch 8-bit (74HC165): DEV_ID (0=Hub,1-63=Slave) + Baud
│       ├── rs485.cpp/.h           ← RS485 USART2 (send/loopback/baud runtime)
│       ├── flashmem.cpp/.h        ← SPI Flash W25Q128 (id/read/write/erase) + persist NET_ID
│       ├── rtc.cpp/.h             ← RTC PCF85063 (I2C)
│       ├── ledbuzz.cpp/.h         ← LED_LIFE, Buzzer, nút nhấn S2
│       ├── rf_link.cpp/.h         ← nRF24L01+RFX2401C (thư viện RF24 + framing riêng, dedup/heartbeat theo dev_id)
│       ├── bridge.cpp/.h          ← CHỨC NĂNG CHÍNH: cầu RS485 <-> Wireless
│       └── watchdog.cpp/.h        ← IWDG watchdog (wrap thư viện IWatchdog) - "wdt stat"
│
└── docs/
    ├── MBW_Test_Procedure.md      ← hướng dẫn test 1 board/máy + quan sát forward
    └── Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md ← thiết kế mạng 1 Hub + tới 64 Slave (DEV_ID/NET_ID, repeater, lộ trình)
```

---

## Firmware (PlatformIO + Arduino/STM32duino)

Một firmware duy nhất — chức năng cầu RS485⇄Wireless **mặc định BẬT ngay khi
cấp nguồn** (không cần lệnh kích hoạt, đây là hành vi chuẩn của sản phẩm khi
lắp đặt thật). Lệnh CLI `bridge on|off` chỉ dùng để kỹ thuật viên tạm dừng khi
cần chạy các lệnh test RS485/RF thủ công khác. Mọi board (Hub lẫn Slave) chạy
**CHUNG 1 bản firmware** — vai trò tự xác định lúc boot qua `dev_id` đọc từ
DIP (xem mục dưới), không cần build/nạp riêng cho Hub.

### Kiến trúc mạng: DEV_ID vs NET_ID (2026-07-04/05)

Thiết kế ban đầu gán `src_id` (định danh nguồn dùng để lọc trùng/theo dõi
heartbeat) **bằng chính Network ID** — đúng cho 1 cặp Hub-Slave, nhưng SAI khi
nhiều Slave dùng chung 1 Network ID: toàn bộ N thiết bị chỉ có 1 định danh
nguồn duy nhất, làm lọc trùng (dedup) và LINK UP/DOWN không phân biệt được
"ai gửi" — có thể **âm thầm loại bỏ frame hợp lệ** của thiết bị này vì trùng số
thứ tự (`seq`) ngẫu nhiên với thiết bị khác. Đã tách thành 2 con số độc lập
(chi tiết đầy đủ + lý do trong
[`docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md`](docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md)):

| | DEV_ID | NET_ID |
|---|---|---|
| Ý nghĩa | Định danh RF **riêng từng board** trong 1 mạng | Hằng số **chung cho cả 1 deployment** (Hub + mọi Slave) |
| Phạm vi | 0-63: `0`=Hub, `1-63`=Slave | 0-63: cách ly nhiều lắp đặt độc lập nằm gần nhau vật lý (vùng phủ RF chồng lấn) |
| Cấu hình | DIP switch SW1-6 (`dip_dev_id()`) — gạt DIP lúc lắp đặt, không cần laptop | CLI `net id <n>` — lưu SPI Flash (W25Q128), giữ qua các lần mất nguồn |
| Liên hệ với địa chỉ Modbus | **Độc lập hoàn toàn** — không cần bằng địa chỉ Modbus thật của slave | Không liên quan |
| Mặc định khi chưa cấu hình | DIP để nguyên (không gạt bit nào) = `0` = Hub | Sentinel `0xFF` (chưa cấu hình) — firmware cảnh báo rõ lúc boot |

Khung tin RF (`rf_frame_t`, `drv_common.h`) mang trường `dev_id` (không tăng
kích thước khung — dùng lại đúng vị trí byte đầu tiên trước đây là `src_id`).
NET_ID không nằm trong payload vì đã ẩn sẵn trong địa chỉ pipe RF — mọi khung
nhận được trên 1 pipe chắc chắn cùng NET_ID.

**2026-07-06 (FW 0.2.0):** thêm 1 byte `hop` vào header (phục vụ Repeater
1-hop, xem mục dưới) — header 5→6 byte, dữ liệu mỗi khung 25→24 byte, số mảnh
tối đa 10→11 (11×24=264 ≥ 250 byte khung Modbus dài nhất, không mất tính
năng). **Đổi cấu trúc khung nghĩa là MỌI board trong cùng 1 mạng phải nạp
cùng phiên bản firmware này** — khung của firmware cũ sẽ bị loại bởi CRC.

**Trước khi lắp đặt nhiều board:** chạy `net id <n>` (cùng giá trị cho Hub +
mọi Slave), gạt DIP `dev_id` không trùng nhau trên từng board (Hub để mặc định
`0`), rồi dùng `rf devices` để xác nhận đúng 1 Hub + không trùng `dev_id` trước
khi đưa vào vận hành thật (tương ứng bước T0 trong tài liệu thiết kế).

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

> **2026-07-05 — bài học RAM khi thêm DEV_ID/NET_ID:** bản đầu tiên của tính
> năng theo dõi heartbeat/dedup riêng theo `dev_id` (mục "Kiến trúc mạng" ở
> trên) dùng `bool[64]` x2 + `uint32_t last_seen_ms[64]` + bảng dedup 68-slot
> kiểu tìm-kiếm-tuyến-tính — build thật trên `pio run` báo
> **`region 'RAM' overflowed by 200 bytes`**. Đã tối ưu lại trong
> `rf_link.cpp`: dedup bỏ hẳn bảng "slot + tìm kiếm" (không cần thiết vì
> `dev_id` đã bị chặn đúng 0-63 = đúng số phần tử mảng, dùng thẳng `dev_id`
> làm chỉ số mảng); 2 mảng `bool[64]` đổi thành bitmap `uint8_t[8]`; mốc thời
> gian theo từng `dev_id` đổi từ `uint32_t` mili-giây (256 byte) sang
> `uint16_t` giây (128 byte, chỉ dùng cho hiển thị CLI chẩn đoán, KHÔNG ảnh
> hưởng thuật toán redundant-TX toàn cục vẫn dùng `millis()` 32-bit như cũ).
> Tổng RAM các mảng này giảm từ ~576 byte xuống **~208 byte**. **Vẫn cần chạy
> lại `pio run` trên máy thật + `rtos stat` để xác nhận đã đủ RAM** trước khi
> coi là xong — sandbox môi trường này không tải được toolchain STM32 thật
> (proxy mạng chặn PlatformIO Registry) nên chỉ rà soát code thủ công, không
> tự build kiểm chứng được.

### Watchdog (IWDG) — tự phục hồi khi treo máy

**2026-07-05:** trước đây firmware KHÔNG có watchdog — nếu 1 trong 3 task RTOS
bị treo thật (deadlock mutex, chờ SPI vô hạn, kẹt trong vòng lặp gửi RF do
phần cứng lỗi...), máy đứng im vô hạn (chỉ có
`vApplicationStackOverflowHook()` nháy LED báo hiệu, KHÔNG tự reset) — rủi ro
thật với thiết bị lắp đặt không người trông coi. Đã thêm IWDG (Independent
Watchdog, chạy bằng LSI nội bộ ~37kHz, không phụ thuộc thạch anh chính) qua
thư viện `IWatchdog` bundled sẵn trong STM32duino core.

**Thiết kế: watchdog theo sức khỏe từng task**, không phải "feed vô điều
kiện" (`src/drivers/watchdog.h/.cpp` + mục watchdog trong `rtos_glue.h/.cpp`):

- Mỗi task (RS485, RF, CLI — `main.cpp`) tự ghi `millis()` vào 1 biến riêng
  (`g_aliveRS485Ms`/`g_aliveRFMs`/`g_aliveCLIMs`) **mỗi vòng lặp** của nó.
- `loop()` (thân của Idle task sau `vTaskStartScheduler()`) là nơi **duy
  nhất** gọi `wdt_feed()` (`IWatchdog.reload()`) — và **chỉ feed khi cả 3
  task đều vừa "điểm danh" trong `RTOS_TASK_ALIVE_TIMEOUT_MS`** (3 giây, xem
  `rtos_all_tasks_alive()`). Nếu **đúng 1 task** bị treo trong khi 2 task còn
  lại vẫn chạy bình thường, cơ chế này vẫn phát hiện được — không chỉ bắt
  được trường hợp treo toàn hệ thống.
- Timeout IWDG = **8 giây** (`wdt_init(8000000UL)` gọi sớm trong `setup()`,
  trước `drv_init()`/tạo task) — dư dả so với thao tác chậm nhất trong
  firmware (xóa sector Flash ~vài trăm ms, redundant TX tối đa 6 lần × nhiều
  mảnh) để không reset nhầm, nhưng vẫn phục hồi được trong thời gian hợp lý
  nếu treo thật.
- Lệnh CLI **`wdt stat`**: `WDT_BOOT_WAS_RESET=yes|no` (lần khởi động trước
  có bị watchdog reset không — chẩn đoán treo máy đã từng xảy ra),
  `WDT_FEEDING=YES|NO` (đang được feed hay sắp bị reset), và tuổi điểm danh
  (ms) của từng task — hữu ích để xác định NHANH task nào đang có vấn đề.

**Lưu ý quan trọng khi debug qua ST-Link:** IWDG một khi đã `begin()` thì
**không thể tắt lại** (đặc tính phần cứng của dòng STM32 này). Nếu halt CPU ở
breakpoint lâu hơn 8 giây, IWDG vẫn đếm và sẽ RESET MCU ngay cả khi đang debug
bình thường — đây là hiện tượng IWDG bình thường, không phải bug code.

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
| **Tắt auto-ack/auto-retry phần cứng**, tự gửi lặp lại (**cố định, mặc định 3 lần** — 2026-07-06) + lọc trùng theo `(dev_id, seq)` | Đây là kênh **broadcast** (nhiều thiết bị cùng NET_ID cùng nghe 1 địa chỉ) — ACK 1-1 không hợp lệ khi có nhiều bên nhận. Gửi lặp lại không cần ACK vẫn tăng xác suất tới nơi trong môi trường nhiễu. Lọc trùng khóa theo `dev_id` **riêng từng board**. **Đã BỎ auto-adapt số lần gửi lặp theo link toàn mạng** (mục 3.1 tài liệu thiết kế: với 16 slave, giây nào cũng có ai đó phát → thuật toán luôn thấy "link tốt", không bao giờ tăng dự phòng cho đúng slave xa đang rớt — bị "trung bình hóa"). Giá trị cố định chỉnh được lúc bench test bằng `rf redund <2-6>`. |
| **Heartbeat + đo chất lượng link** (lấy ý tưởng từ bộ telemetry MAVLink/SiK radio) | Mỗi board tự phát 1 khung điều khiển nhỏ mỗi giây (không đụng vào dữ liệu Modbus) để bên kia biết "còn sống"; nếu quá 3s không nhận được gì (kể cả dữ liệu thật) thì báo `RF LINK: DOWN`. **LOSS‰ đo riêng theo từng `dev_id`** (2026-07-06, xem `rf devices`) — biết chính xác thiết bị nào rớt nhiều để tối ưu vật lý hoặc bật repeater đúng chỗ. |
| **Repeater 1-hop** (2026-07-06, mục 6 tài liệu thiết kế) | Board bật `rf repeater on` (hoặc giữ nút S2 3 giây) phát lại khung của node KHÁC đúng 1 lần (byte `hop` trong header giảm 1 mỗi lần relay, hết hop không relay tiếp — chống lặp vô hạn). Relay đối xứng cả 2 chiều (poll lẫn response, cả heartbeat), giữ nguyên `dev_id`/`seq` gốc nên bên nhận coi như 1 lần "gửi lặp" bình thường, dedup sẵn có tự lọc. Chỉ relay bản ĐẦU TIÊN của mỗi `(dev_id, seq)` — không nhân relay theo số lần gửi lặp của node gốc. |

### Heartbeat + giám sát chất lượng link RF

Board tự gửi 1 khung điều khiển "heartbeat" mỗi 1 giây (`RF_HB_PERIOD_MS`), tách
biệt hoàn toàn với dữ liệu Modbus thật (dùng giá trị `frag_idx=0xFF` làm dấu
hiệu nhận biết, không thể trùng với khung dữ liệu thật). Cơ chế:

- **Bất kỳ khung hợp lệ nào** (dữ liệu thật hay heartbeat) đều tính là "còn nghe
  thấy nhau" — không cần đợi đúng heartbeat mới coi là link sống.
- Quá `RF_LINK_TIMEOUT_MS` (3 giây, ~3 chu kỳ heartbeat) không nhận được gì →
  in `RF LINK: DOWN (peer=<id>, <n>ms...)`. Khi nhận lại được → in
  `RF LINK: UP (peer=<id>)`. Giống cách Mission Planner báo "mất kết nối" khi
  hết heartbeat MAVLink. Đây là trạng thái **gộp toàn mạng** (link sống nếu
  BẤT KỲ thiết bị nào còn phát) — dùng riêng cho thuật toán redundant-TX tự
  thích ứng bên dưới.
- **Theo dõi riêng từng `dev_id`** (2026-07-05, xem mục "Kiến trúc mạng"): vì
  N thiết bị cùng chia sẻ 1 NET_ID, biết "link chung còn sống" không nói lên
  được đúng thiết bị nào đang mất sóng. Firmware log riêng biệt
  `RF LINK: UP (dev_id=<n>)` / `RF LINK: DOWN (dev_id=<n>, <n>s...)` cho từng
  thiết bị, đọc qua lệnh CLI `rf devices` (liệt kê toàn bộ) hoặc
  `rf dev <id>` (1 thiết bị). Để tiết kiệm RAM (chip chỉ 10KB), mốc thời gian
  theo từng `dev_id` lưu ở độ phân giải **giây** (không phải mili-giây như
  trạng thái toàn mạng) — đủ dùng vì ngưỡng timeout là 3 giây.
- **Redundant TX cố định** (2026-07-06 — thay cho "tự thích ứng" trước đây):
  số lần gửi lặp giữ nguyên 1 giá trị đã kiểm chứng (mặc định 3), đổi lúc
  bench test bằng `rf redund <2-6>` (chỉ RAM, về mặc định sau reset). Lý do bỏ
  auto-adapt: với 16 slave cùng NET_ID, chỉ số "kỳ mất trắng" toàn mạng gần
  như luôn tốt (ai đó luôn đang phát) nên thuật toán không bao giờ tăng dự
  phòng cho đúng slave yếu — số liệu sai còn tệ hơn giá trị cố định. Lớp bảo
  vệ cuối vẫn là timeout/retry của Modbus master (khuyến nghị 1000-1500ms,
  ≥1500ms khi có repeater).
- **LOSS‰ theo từng `dev_id`** (2026-07-06, mục 7.2 tài liệu thiết kế): đếm
  heartbeat nhận được từ từng thiết bị so với số giây trôi qua từ lần
  `rf reset` gần nhất (mỗi thiết bị phát đúng 1 heartbeat/giây). Quy trình đo:
  cấp nguồn đủ mọi board → `rf reset` → chờ ≥60s → `rf devices` (cột `loss%o`).
  Ngưỡng theo `MBW_Test_Procedure.md`: TỐT <20‰, CHẤP NHẬN 20-100‰, KÉM >100‰.
- Lệnh `rf stat` hiển thị đầy đủ: `RF_LINK=UP|DOWN LAST_DEV_ID=<id> AGE_MS=<n>
  LOSS_PROMILLE=<n> REDUND=<n> HB_TX=<n> HB_RX=<n> REPEATER=ON|OFF RELAY=<n>
  RF_NETID=<n> RF_DEVID=<n>` (trạng thái toàn mạng + NET_ID/DEV_ID của chính
  board). Dùng `rf devices` / `rf dev <id>` để xem UP/DOWN + LOSS‰ **theo
  từng thiết bị**.
- App test (`mbw_test_app.py`, tab Giám sát Forward) có nhãn **RF Link** hiển
  thị UP/DOWN theo thời gian thực (đọc dòng log tức thời) + nút "Đọc RF Link
  (rf stat)" để xem % mất và độ dự phòng hiện tại — tự động làm mới mỗi 3 giây
  khi đang kết nối.

### Lệnh CLI (Console USART1, 115200)

| Lệnh | Chức năng |
|---|---|
| `id` / `ver` / `help` | ID thiết bị / phiên bản FW / danh sách lệnh |
| `led` / `beep on\|off` | Toggle LED_LIFE / tắt-bật tiếng bíp |
| `dip` | Đọc DIP switch: `DEVID` (0-63, kèm `(HUB)`/`(SLAVE)`) + `BAUD` (4800/9600/14400/19200) |
| `rs485 <text>` | Gửi thử 1 chuỗi ra RS485 |
| `rsl` | Loopback RS485 (cần nối tắt A-B) |
| `rs485mon on\|off` | Forward RX RS485 ra console (KHÔNG bật cùng lúc với `bridge on`) |
| `baud rs485 <bps>` | Đổi baudrate RS485 lúc chạy |
| `flash` / `fwr` | Đọc JEDEC ID / test ghi-đọc SPI Flash W25Q128 |
| `rtc` / `rtc set <hh:mm:ss>` | Đọc/đặt giờ RTC PCF85063 |
| `net id <0-63>` / `net id` | Ghi/đọc **NET_ID** — hằng số chung cho cả deployment, **lưu Flash** (giữ qua các lần mất nguồn), áp dụng ngay không cần reset board |
| `rf id` | Kiểm tra nRF24L01 có mặt (SPI) |
| `rf ch <0-125>` | Đặt kênh RF (mặc định 120, vùng tránh WiFi 1-11) |
| `rf netid <0-63>` | Ghi đè NET_ID **tạm thời** (chỉ trong RAM, KHÔNG lưu Flash) — dùng `net id` ở trên để lưu vĩnh viễn |
| `rf tx <text>` | Gửi 1 bản tin không dây thủ công (kỹ thuật viên tự kiểm tra RF thô, cần `bridge off` tạm thời) |
| `rf stat` / `rf reset` | Thống kê TX/RX OK/trùng/lỗi CRC/rớt mảnh + kênh/NET_ID/DEV_ID + **LINK UP/DOWN toàn mạng, % mất, REDUND cố định, REPEATER/RELAY**. `rf reset` đồng thời đặt lại cửa sổ đo LOSS‰ theo dev_id |
| `rf devices` | Liệt kê mọi `dev_id` **đã từng nghe thấy** + vai trò (HUB/SLAVE) + UP/DOWN + số giây từ lần nghe cuối + **LOSS‰ riêng từng thiết bị** — dò trùng lặp `dev_id`/thiếu Hub lúc nghiệm thu, tìm slave yếu lúc khảo sát |
| `rf dev <0-63>` | Xem chi tiết link **1 `dev_id` riêng lẻ** (UP/DOWN + số giây từ lần nghe cuối + LOSS‰) |
| `rf redund <2-6>` | Đổi số lần gửi lặp **cố định** lúc bench test (chỉ RAM, mặc định 3 sau reset) |
| `rf repeater on\|off` / `rf repeater` | Bật/tắt **chế độ Repeater 1-hop** (lưu Flash, giữ qua mất nguồn) — hoặc **giữ nút S2 3 giây** (LED nháy 3 lần, bíp dài=BẬT/ngắn=TẮT, không cần laptop) |
| `bridge on\|off` | Tạm BẬT/TẮT chức năng cầu RS485⇄Wireless (**mặc định ON ngay khi cấp nguồn**) |
| `bridge log on\|off` | Bật/tắt in dòng `FWD ...` khi relay (**mặc định OFF từ FW 0.2.0** — bật khi test/quan sát bằng app) |
| `bridge stat` / `bridge reset` | Thống kê số khung đã relay 2 chiều + trạng thái bridge/log + số khung bị rớt do hàng đợi liên-task đầy (`DROP_RS485_TO_RF`/`DROP_RF_TO_RS485`) |
| `rtos stat` | RAM heap còn trống (`xPortGetFreeHeapSize`) + stack còn trống từng task RS485/RF/CLI — kiểm tra ngân sách RAM RTOS trên phần cứng thật |
| `wdt stat` | Trạng thái watchdog IWDG: lần khởi động trước có bị reset do treo máy không (`WDT_BOOT_WAS_RESET`), đang được feed hay sắp reset (`WDT_FEEDING`), tuổi điểm danh từng task — xem mục "Watchdog (IWDG)" |

Khi `bridge off`, mọi bản tin RF nhận được sẽ tự in ra console dạng
`RF RX: <nội dung>` — dùng để kiểm tra RF thô giữa 2 board mà không qua bridge.

### Quy trình lắp đặt 1 Hub + 16 Slave & khi nào bật Repeater (2026-07-06)

1. **Chuẩn bị từng board:** nạp cùng FW 0.2.0 cho MỌI board (đổi cấu trúc
   khung — không trộn firmware cũ/mới); `net id <n>` cùng giá trị cho cả
   deployment; gạt DIP `dev_id`: board nối Master để nguyên (0=Hub), board nối
   slave gạt 1..16 không trùng nhau.
2. **T0 — nghiệm thu mạng:** trên board Hub chạy `rf devices` — phải thấy đủ
   16 slave UP, đúng 1 Hub, không trùng `dev_id`.
3. **Đo chất lượng từng link:** `rf reset` → chờ ≥60s → `rf devices`, đọc cột
   `loss%o` từng slave (TỐT <20‰ / CHẤP NHẬN 20-100‰ / KÉM >100‰).
4. **Slave nào KÉM:** tối ưu vật lý trước (đổi kênh `rf ch`, hướng anten, vị
   trí lắp) — đo lại bước 3. Đây là bước bắt buộc trước khi nghĩ đến repeater
   (mục 7.3 tài liệu thiết kế).
5. **Vẫn kém do xa/khuất:** chọn 1 board trung gian (slave sẵn có ở giữa hoặc
   board bridge đặt thêm), bật `rf repeater on` (hoặc giữ nút S2 3 giây).
   **Chỉ 1 repeater cho 1 khu vực** — nhiều repeater cùng nghe nhau sẽ chiếm
   kênh vô ích. Tăng timeout Modbus master lên ≥1500ms (thêm ~1 hop trễ mỗi
   chiều). Đo lại bước 3 để xác nhận.
6. **Giám sát định kỳ:** slave "sống nhờ repeater" vẫn hiện UP — nên đọc
   `rf stat` trên board repeater (`RELAY=<n>` tăng đều nghĩa là nó đang thật
   sự gánh link), và kiểm tra lại LOSS‰ định kỳ để phát hiện link trực tiếp
   xấu thêm.

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

- **DIP switch — cực tính ĐÃ đối chiếu trên board thật (2026-07-06):** gạt
  switch ON = nối GND → mức điện 0 (chân 74HC165 có pull-up); đọc thô all-OFF
  ra `0xFF` (DEVID=63) là sai quy ước "DIP để nguyên = 0 = HUB". Đã **đảo bit
  trong `dip_read_raw()`** — từ nay all-OFF = `0x00` = DEVID 0 (HUB), BAUD
  4800 (SW7-8 = 00). **Lưu ý baud khi lắp đặt:** SW7-8 đều OFF nghĩa là RS485
  chạy 4800 — muốn 9600 gạt SW7 ON (01), 19200 gạt cả SW7+SW8 ON (11).
  **Thứ tự bit** (SW1-8 ↔ ngõ A-H) vẫn theo giả định bit0-5=SW1-6, bit6-7=SW7-8
  — cần xác nhận thêm bằng cách gạt riêng SW1 xem DEVID có ra 1 không.
- **NET_ID lưu Flash**: chưa xác nhận trên phần cứng thật việc `net id <n>`
  ghi xuống rồi đọc lại đúng sau khi rút nguồn/cấp lại (địa chỉ
  `NET_ID_FLASH_ADDR = 0x001000`, `flashmem.h`) — nên test trước khi lắp đặt
  hàng loạt.
- **T0 (xác nhận mạng trước khi vận hành thật)**: dùng `net id` (đọc lại từng
  board xem cùng giá trị), `rf devices` (đúng 1 `dev_id=0`, không trùng
  `dev_id` khác) — xem mục "Kiến trúc mạng" và
  [`docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md`](docs/Dinh_Huong_Mo_Rong_64_Node_ModbusRTU.md)
  mục 3.2.f để biết đầy đủ tiêu chí đạt.
- **TXEN/RXEN của RFX2401C**: tài liệu giả định đấu cứng theo PA8/RF_CE (không
  cần firmware can thiệp) — nên đo bằng oscilloscope lúc TX/RX để xác nhận.
  Nếu KHÔNG tự chuyển mạch theo CE, cần bổ sung GPIO điều khiển riêng trong
  `rf_link.cpp`.
- **Kênh RF mặc định (120)**: bảng kênh trên trang sản phẩm epcb.vn ghi
  2.520–2.525 (GHz) ứng với kênh nRF24 120-125 — chưa thấy switch/jumper chọn
  kênh riêng trên schematic, hiện cố định trong firmware (`rf ch` để đổi thủ
  công lúc test). Cần xác nhận cách chọn kênh thực tế của sản phẩm.
- **Log forward `FWD ...`**: từ FW 0.2.0 mặc định `bridge log off` (đã áp
  dụng khuyến nghị này cho lắp đặt thật) — app test/kỹ thuật viên bật lại bằng
  `bridge log on` khi cần quan sát (app có sẵn nút toggle).
- **RAM sau FW 0.2.0**: LOSS‰ theo dev_id (+128B), buffer ghép mảnh 250→264B
  (×2, +28B) — tổng tăng ~160B so với 0.1.x, bù lại bỏ biến auto-adapt. Biên
  RAM đã từng chỉ dư ~168B (xem "bài học RAM" ở trên) nên **bắt buộc `pio run`
  trên máy thật xem có tràn RAM không + `rtos stat` sau khi nạp** — nếu thiếu,
  giảm `RTOS_STACK_xxx` trước.
- **`_reference/` chứa firmware/app các đợt trước** (đối chiếu pin map / luồng
  test cũ của OPMS & Smart PDU) — không phải code của board này.
- **RAM RTOS trên phần cứng thật**: `configTOTAL_HEAP_SIZE=4KB` + stack từng
  task (`rtos_glue.h`). Đã từng gặp `region 'RAM' overflowed by 200 bytes` lúc
  link (xem ghi chú "bài học RAM" trong mục RAM budget ở trên) do mảng
  dev_id/dedup ban đầu quá lớn — đã tối ưu lại. Sau khi `pio run -t upload`
  thành công, vẫn nên dùng lệnh `rtos stat` để xác nhận heap còn trống > 0 và
  stack từng task không về gần 0 — nếu thiếu, giảm `RTOS_STACK_RS485/RF/CLI`
  trước khi tăng `configTOTAL_HEAP_SIZE`.
