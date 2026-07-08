# Báo cáo đánh giá mất frame RS485/RF — Log_Poll_1000ms

**Ngày test:** 07/07/2026
**Nguồn dữ liệu:**
- `Log_Poll_1000ms/fwd_log_20260707_135645.xlsx` (sheet `FWD log` — 37.334 frame raw; sheet `Link 5s` — thống kê tích lũy mỗi 5s)
- `Log_Poll_1000ms/ModbusLog_20260707_135637.xlsx` (sheet `Log` — 1.666 phép đo ứng dụng)

**Thời lượng theo dõi:** ~5.323 giây (~88.7 phút), từ 13:56:52 đến 15:25:35.

## 1. Mức độ mất frame

| Chỉ số | Giá trị |
|---|---|
| Tổng lệnh RS485→RF | 19.071 |
| Tổng phản hồi RF→RS485 | 17.965 |
| Số frame hụt (không khớp) | 1.106 (~5.8%) |
| Dải lỗi theo từng cửa sổ 5s | 2.86% – 8.23% (trung bình 4.68%) |
| Phép đo ứng dụng lỗi (ModbusLog "LỖI") | 127 / 1.666 (~7.6%) |
| CRC lỗi (raw, cả 2 chiều) | 327 |
| Frame ngắn "NGẮN xB" (raw, cả 2 chiều) | 415 |
| Frame quá dài / bỏ qua >16B | 0 |
| Trạng thái RF LINK | UP suốt phiên (không rớt liên kết) |

Tỷ lệ lỗi ổn định quanh 5-8% trong suốt phiên, không có xu hướng tăng dần theo thời gian — cho thấy đây là lỗi mang tính hệ thống/liên tục, không phải suy giảm dần (ví dụ do nhiệt hoặc pin yếu).

## 2. Hai cơ chế lỗi khác nhau theo chiều truyền

### 2.1. Chiều RS485 → RF (lệnh từ master) — lỗi tách khung ở bridge

Thống kê raw: 171 CRC lỗi + 178 frame ngắn ở chiều này.

Đặc điểm: hầu hết các frame lỗi là **frame bị mất byte đầu**, không phải lỗi do nhiễu đường dây. Ví dụ:

- `6A 00 01 A4 16` (5 byte) — là phần đuôi của lệnh đầy đủ `01 03 6A 00 01 A4 16`, bị rụng 2 byte đầu (`01 03`).
- `04 69 00 01 54 16 01 03 00 69 00 01 54 16` (14 byte) — mảnh vụn còn sót của lệnh trước (`69 00 01 54 16`) bị dính liền vào lệnh kế tiếp vốn nguyên vẹn (`01 03 00 69 00 01 54 16`).
- `01 64 00 01 C5 D5 01 03 00 64 00 01 C5 D5` (14 byte) — cùng mẫu: đuôi lệnh cũ dính vào đầu lệnh mới.

**Đánh giá:** mẫu lỗi lặp lại đều đặn theo kiểu "thiếu byte đầu / dính sang frame sau" là dấu hiệu điển hình của bộ tách khung (frame delimiter theo khoảng lặng/timeout) trên bridge đóng buffer nhận RS485 trễ, hoặc mở lại buffer quá sớm — khiến vài byte đầu của lệnh kế tiếp bị nuốt và trôi dạt sang lần đọc trước.

→ **Nguyên nhân nghi ngờ:** vấn đề timing ở logic nhận UART/RS485 phía bridge (inter-frame timeout / silence-detection), không phải do nhiễu vật lý trên đường RS485.

### 2.2. Chiều RF → RS485 (phản hồi từ sensor) — lỗi bit/gói trên sóng RF

Thống kê raw: 156 CRC lỗi + 237 frame ngắn ở chiều này.

Đặc điểm: lỗi chủ yếu là **CRC sai trên frame đúng độ dài** (7 byte), lệch đúng 1 byte, ví dụ:

- `01 03 02 00 00 B8 FE` (đúng) → `01 03 02 00 00 B8 44` (nhận được, lệch byte cuối)
- `01 03 02 01 2B F9 CB` (đúng) → `01 03 02 01 2B F9 FF` (nhận được, lệch byte cuối)

Ngoài ra còn nhiều frame bị cắt còn 1 byte lẻ (`F1`, `F9`, `E1`...) — có vẻ là tàn dư cuối gói bị mất phần đầu.

**Đánh giá:** đây là dấu hiệu lỗi bit/gói thực sự trên sóng RF (nRF24) — nhiễu kênh 2.4GHz, suy hao tín hiệu, hoặc gói bị ngắt giữa chừng — không phải lỗi logic khung.

## 3. Kết luận & đề xuất

Mất frame (~5.8%) không đến từ một nguyên nhân duy nhất mà từ **hai nguồn lỗi song song, độc lập**:

1. **Lỗi tách khung RS485 phía bridge (chiều lệnh):** cần rà lại timeout ngắt khung (inter-frame gap) và logic reset buffer nhận RS485 trên bridge — đây là lỗi có thể sửa bằng firmware.
2. **Chất lượng liên kết RF ở mức biên (~5-8% mất gói) (chiều phản hồi):** cần kiểm tra khoảng cách anten, nguồn nhiễu 2.4GHz xung quanh (WiFi, Bluetooth), và cân nhắc bổ sung cơ chế retry/ACK ở tầng RF nếu hiện chưa có, để giảm phụ thuộc vào retry ở tầng ứng dụng Modbus.

RF LINK báo "UP" liên tục nên đây không phải hiện tượng rớt kết nối, mà là lỗi/mất gói rải rác trong khi liên kết vẫn "sống".

## 4. Cập nhật 2026-07-07: đã xác định và sửa nguyên nhân gốc phía RS485 (bước 1)

Đọc lại source firmware (`src/drivers/bridge.cpp`, `src/main.cpp`, `src/rtos_glue.*`) xác nhận đúng giả thuyết ở mục 2.1, với một chi tiết quan trọng: **log "FWD ..." dùng để ghi ra 2 file log trong báo cáo này (`bridge log on`) chính là thứ gây ra phần lớn lỗi tách khung**, không chỉ là do timing ngẫu nhiên.

Cụ thể: `bridge_rs485_step()` chạy trên task RS485 — task có độ ưu tiên CAO NHẤT trong hệ RTOS, có nhiệm vụ duy nhất là gom byte và phát hiện khoảng lặng 3.5 ký tự (tối thiểu 1.75ms) để tách khung Modbus. Trước bản sửa này, mỗi khi relay xong 1 khung, hàm `log_forward()` gọi `SerialDBG.print(...)` (in ra console debug) **ngay trong task đó**, đang giữ mutex `g_muSerial`. In 1 dòng "FWD RS485->RF: 8 bytes: ..." tốn khoảng vài mili-giây tùy baud debug UART — **vượt quá ngưỡng 1.75ms** dùng để tách khung. Trong lúc đang in, byte đầu của khung Modbus kế tiếp đã tới thì bị đọc dồn vào cùng khung đang xử lý → đúng là hiện tượng "mất byte đầu / dính 2 khung làm 1" quan sát được trong log.

Vì file log RAW mà báo cáo này phân tích được ghi bằng cách bật "bridge log on", nhiều khả năng **tỷ lệ mất frame đo được (~5.8%) bị đội lên** so với vận hành thực tế (mặc định `bridge log` đang TẮT — xem comment gốc trong code).

**Đã sửa (firmware, chưa build/nạp/kiểm định trên board thật):**
- Tách việc *in* ra khỏi task RS485: task RS485 giờ chỉ đẩy (enqueue) một struct nhỏ vào hàng đợi `g_qLog` mới (không bao giờ block — nếu hàng đợi đầy thì bỏ qua dòng log đó, chấp nhận được vì chỉ là log debug).
- Việc in thật sự (`SerialDBG.print`) chuyển sang hàm mới `bridge_log_process()`, gọi từ task CLI — task có độ ưu tiên THẤP NHẤT, không còn ảnh hưởng đến việc tách khung Modbus nữa.
- File đã sửa: `src/rtos_glue.h`, `src/rtos_glue.cpp`, `src/drivers/bridge.cpp`, `src/drivers/bridge.h`, `src/main.cpp`.

**Việc cần làm tiếp:**
1. Build (`pio run`) và nạp thử trên board thật, chạy lại đúng kịch bản Poll 1000ms với `bridge log on` để đo lại tỷ lệ mất frame — kỳ vọng giảm đáng kể phần lỗi tách khung RS485 (mục 2.1).
2. Sau khi có số liệu mới, tách được phần lỗi còn lại thực sự thuộc về RF (mục 2.2) để quyết định mức đầu tư cho retry/ACK RF (bước 2 trong kế hoạch tối ưu).
3. (Theo dõi thêm) `rs485_process()` ("rs485mon") trong `src/drivers/rs485.cpp` vẫn còn in trực tiếp trên task RS485 theo kiểu cũ — nhưng chỉ dùng khi bật debug echo riêng (không dùng đồng thời với bridge), nên rủi ro thấp hơn nhiều so với lỗi vừa sửa; có thể áp dụng cùng cách vá nếu cần.

### Cập nhật: build lần 1 lỗi RAM, đã vá lại (cùng ngày)

Bản vá đầu tiên dùng 1 hàng đợi FreeRTOS (`xQueueCreateStatic`) để chuyển dữ liệu log từ task RS485 sang task CLI. Build thật báo lỗi:

```
region `RAM' overflowed by 88 bytes
```

Nguyên nhân: chip STM32L151C8T6 chỉ có 10KB RAM (đã được quản lý rất chặt trong code, từng phải giảm stack từng 16-32 byte một), và cấu trúc `StaticQueue_t` nội bộ của FreeRTOS tốn khoảng 70-80 byte "control block" (con trỏ, linked-list nội bộ...) cho **mỗi** hàng đợi tạo ra — quá đắt chỉ để chuyển 1 dòng debug.

**Đã vá lại:** bỏ hàng đợi FreeRTOS, thay bằng **ring buffer tự viết tay** (single-producer/single-consumer) ngay trong `bridge.cpp` — task RS485 là producer duy nhất (chỉ ghi biến `head`), task CLI là consumer duy nhất (chỉ ghi biến `tail`), không cần mutex/queue object vì đọc/ghi 1 biến 8-bit riêng của mỗi bên là an toàn trên Cortex-M. Đồng thời giảm preview log từ 16 xuống 8 byte và độ sâu hàng đợi còn 2, giúp giảm thêm RAM tiêu thụ. Tổng RAM thêm vào ước tính giảm từ ~110+ byte xuống còn ~30-40 byte.

File thay đổi thêm trong lần vá này: `src/rtos_glue.h`, `src/rtos_glue.cpp` (bỏ `g_qLog`), `src/drivers/bridge.cpp` (thêm ring buffer `log_queue_push()`/`log_queue_pop()`), `src/drivers/bridge.h` (cập nhật comment).

**Vẫn cần bạn build lại (`pio run`) để xác nhận hết lỗi RAM trước khi nạp xuống board.**

## 5. Kết quả retest sau khi nạp firmware mới (17:50, 07/07/2026)

**Nguồn dữ liệu:** `docs/File_Log_17h-07-07-2026/fwd_log_20260707_175018.xlsx` + `ModbusLog_20260707_175030.xlsx`. Thời lượng: ~4 phút (17:50:29 → 17:54:29), ngắn hơn nhiều so với lần đo đầu (~88 phút) — nên số liệu tổng nhiễu hơn, cần xem đúng từng phần dưới đây.

### 5.1. Lỗi tách khung/ghép khung RS485 (mục 2.1) — ĐÃ HẾT

Đây là điều quan trọng nhất cần xác nhận: trước fix, frame lỗi phía RS485→RF thường dài 12-16 byte (dấu hiệu 2 khung bị dính làm 1, ví dụ `04 69 00 01 54 16 01 03 00 69 00 01 54 16`). Sau fix, quét toàn bộ 15 frame lỗi phía RS485→RF trong log mới: **độ dài lớn nhất chỉ còn 7 byte**, không còn bất kỳ frame nào dài 12-16 byte kiểu ghép 2 khung. Ví dụ frame lỗi còn lại: `01 03 00 65 00` (5 byte, thiếu CRC), `01 03 00 67 00 01 F5` (7 byte, CRC sai) — đều là lỗi 1 khung đơn lẻ, không phải ghép khung.

→ **Xác nhận: đúng nguyên nhân "chặn task RS485 do in log" đã được loại bỏ.**

### 5.2. Tỷ lệ mất frame tổng thể — CHƯA CẢI THIỆN (thậm chí cao hơn) trong lần đo ngắn này

| Chỉ số | Lần đo trước (88 phút) | Lần đo mới (4 phút) |
|---|---|---|
| Lỗi tổng % (trung bình cửa sổ 5s) | 4.68% | **9.41%** |
| ModbusLog "LỖI" (ứng dụng) | 7.6% (127/1666) | 6.3% (5/79) |
| RS485→RF: frame lỗi/tổng | 349/19218 (1.82%) | 15/1100 (1.36%) — **giảm nhẹ** |
| RF→RS485: frame lỗi/tổng | 393/18116 (2.17%) | 35/987 (3.55%) — **tăng** |

Tỷ lệ lỗi tổng thể trong lần đo mới KHÔNG giảm — thậm chí cao hơn lần trước. Tuy nhiên đây gần như chắc chắn KHÔNG phải do fix vừa rồi gây ra thêm lỗi, vì:
- Lỗi phía RS485 (đúng chỗ vừa sửa) đã giảm nhẹ (1.82%→1.36%) và đặc biệt đã HẾT hẳn kiểu lỗi ghép khung (mục 5.1) — đúng như kỳ vọng.
- Phần tăng lên nằm ở chiều RF→RS485 (phía sóng RF, mục 2.2) — phần này KHÔNG bị đụng tới trong fix hôm nay. Vẫn cùng dạng lỗi cũ: CRC sai lệch 1 byte trên frame đúng độ dài (`01 03 02 00 00 B8 FC`) và frame cụt còn 1 byte (`F1`, `E1`) — đặc trưng của nhiễu/suy hao RF, không phải lỗi logic khung.
- Log mới chỉ dài 4 phút (so với 88 phút lần trước) nên rất dễ bị nhiễu bởi điều kiện RF tức thời (khoảng cách, vật cản, nhiễu 2.4GHz lúc đo) — không đủ dữ liệu để so sánh tỷ lệ tổng một cách công bằng.

**Kết luận bước này (đã bị lần đo 30 phút ở mục 6 phủ định — xem bên dưới):** kết luận "đã hết lỗi ghép khung" ở trên dựa trên mẫu quá ngắn (4 phút, chỉ 15 frame lỗi) nên không đáng tin cậy - lỗi ghép khung THỰC RA VẪN CÒN, xem mục 6.

## 6. Retest lần 2, ~30 phút (18:00-18:26, 07/07/2026) — lỗi ghép khung VẪN CÒN, không phải do log debug

**Nguồn dữ liệu:** `docs/File_Log_18h-07-07-2026/fwd_log_20260707_180041.xlsx` + `ModbusLog_20260707_180052.xlsx`. Thời lượng ~25.7 phút (1541.9s) — đủ dài để so sánh công bằng với lần đo đầu (88 phút).

### 6.1. Số liệu tổng

| Chỉ số | Lần đo đầu (88 phút, TRƯỚC fix) | Lần đo 30 phút (SAU fix) |
|---|---|---|
| Lỗi tổng % (trung bình cửa sổ 5s) | 4.68% | 5.56% |
| ModbusLog "LỖI" (ứng dụng) | 7.6% (127/1666) | 9.7% (56/577) |
| Số frame RS485→RF bị merge (ghép 2 khung, dài 12-16 byte) | 59 / 19218 khung | 17 / 6816 khung |
| **Tần suất merge/giờ** | **39.9 lần/giờ** | **39.7 lần/giờ** |

### 6.2. Phát hiện quan trọng: fix bước 1 KHÔNG làm giảm tần suất lỗi ghép khung

Tần suất lỗi ghép khung (39.9/giờ trước fix vs 39.7/giờ sau fix) **gần như giống hệt nhau**. Điều này chứng minh: việc tách log "FWD ..." ra khỏi task RS485 (bước 1 đã làm) tuy đúng và nên giữ lại, nhưng **KHÔNG PHẢI** là nguyên nhân chính/duy nhất gây ghép khung. Sau fix, `log_forward()` trong task RS485 chỉ còn copy 8 byte vào ring buffer (vài micro-giây), không còn gọi `SerialDBG.print` nữa — nên nếu log-blocking từng là nguyên nhân chính, tần suất phải giảm mạnh. Việc tần suất không đổi cho thấy có nguồn gây "block" task RS485 KHÁC, độc lập với việc bật/tắt log.

**Nghi phạm mới (đọc lại `src/drivers/rs485.cpp`):** hàm `rs485_send()` — được gọi TỪ CHÍNH task RS485 (trong `bridge_rs485_step()`, chiều RF→RS485) — có `SerialRS485.flush()`, đây là lệnh **BLOCK CHO ĐẾN KHI TOÀN BỘ FRAME PHẢN HỒI ĐÃ TRUYỀN XONG THẬT SỰ** qua UART. Với 1 frame phản hồi ~7-8 byte, thời gian block này là 10 bit/byte × 8 byte / baud - ví dụ ở baud 9600 là **~8.3ms**, ở 19200 là **~4.2ms** — đều VƯỢT XA ngưỡng 1.75ms dùng để tách khung Modbus. Trong lúc `flush()` đang block, task RS485 hoàn toàn không quay lại vòng lặp đọc RS485 RX cho chiều RS485→RF - đúng cơ chế có thể gây mat/ghep byte dau cua lenh ke tiep, khop voi mau loi quan sat duoc (vi du `01 00 01 84 0A 01 03 00 ...`: mau vun cuoi 1 khung + khung moi nguyen ven dinh lien).

Đây là ứng viên nguyên nhân gốc hợp lý hơn cho lỗi ghép khung còn sót lại, vì:
- Xảy ra ở MỌI lần bridge relay phản hồi ra RS485 (không phụ thuộc log bật/tắt) → giải thích tại sao tần suất không đổi sau fix bước 1.
- Thời lượng block tỷ lệ với độ dài frame/baud - đúng cỡ ms cần thiết để vượt ngưỡng 1.75ms.

## 7. Đã sửa (2026-07-07, cùng ngày): `rs485_send()` không còn block task RS485

**Cách sửa:** thêm 3 hàm non-blocking trong `src/drivers/rs485.cpp`/`rs485.h`:
- `rs485_send_start(data, len)` — bật DIR=HIGH, gọi `SerialRS485.write()` (không gọi `flush()`), ước lượng thời gian truyền cần thiết theo baud hiện tại (số byte × 10 bit/byte / baud + biên an toàn ~300µs), rồi trả về NGAY LẬP TỨC.
- `rs485_send_poll()` — gọi mỗi vòng lặp task, không block; khi thời gian ước lượng đã đủ thì tự chuyển DIR về LOW (kết thúc gửi) và trả về true đúng 1 lần.
- `rs485_send_pending()` — báo đang có 1 lượt gửi dở dang.

`bridge_rs485_step()` (chiều RF→RS485) đổi từ gọi `rs485_send()` (blocking) sang `rs485_send_start()` + để `rs485_send_poll()` tự hoàn tất dần qua các vòng lặp kế tiếp (mỗi vòng ~1ms) — không còn "đóng băng" việc gom/tách khung chiều RS485→RF trong lúc gửi phản hồi ra nữa. Trong lúc đang tự gửi (`rs485_send_pending()==true`), chiều RS485→RF được CHỦ ĐỘNG bỏ qua (không đọc RS485 RX) — vì lúc đó bus do chính mình chiếm giữ, vật lý không thể có gì thật từ master tới, và tránh đọc nhầm byte "echo" của chính mình.

Hàm `rs485_send()` cũ (blocking) vẫn giữ nguyên, chỉ đổi cách hiện thực bên trong (gọi lại 3 hàm trên trong 1 vòng lặp chờ) — dùng cho lệnh CLI `rs485 <text>` và `rs485_loopback()`, những chỗ KHÔNG nằm trong đường tách khung Modbus nên block vẫn an toàn.

File thay đổi: `src/drivers/rs485.cpp`, `src/drivers/rs485.h`, `src/drivers/bridge.cpp`, `src/drivers/bridge.h`.

**Việc cần làm tiếp:**
1. Build (`pio run`) — thay đổi lần này chỉ thêm ~10 byte RAM (2-3 biến tĩnh), không có rủi ro RAM như lần trước.
2. Nạp lại firmware (cả 2 board — vẫn cùng 1 file build, xem giải thích trước đó), chạy lại đúng kịch bản Poll 1000ms, thời lượng nên ≥ 20-30 phút để so sánh công bằng.
3. Chỉ tiêu để đánh giá thành công: tần suất frame RS485→RF bị ghép 2 khung (dài 12-16 byte) phải GIẢM RÕ RỆT so với mức ~40 lần/giờ hiện tại (lý tưởng gần 0) — đây là chỉ số quyết định, không phải "Lỗi tổng %" chung (chỉ số này còn bị ảnh hưởng nhiều bởi điều kiện RF tức thời, dễ nhiễu giữa các lần đo ngắn).

### Cập nhật: build lần 2 lỗi RAM (8 byte), đã vá lại

Build thật báo tiếp `region 'RAM' overflowed by 8 bytes` — do 3 hàm `rs485_send_start()/rs485_send_poll()/rs485_send_pending()` thêm ~9-12 byte biến tĩnh mới vào `rs485.cpp` (bool + 2 uint32_t theo dõi trạng thái TX), vượt đúng 8 byte so với ngân sách RAM còn lại.

**Đã vá:** giảm kích thước preview log (`BRIDGE_LOG_PREVIEW_MAX` trong `bridge.cpp`) từ 8 xuống 4 byte — tiết kiệm đúng 8 byte (2 phần tử hàng đợi × 4 byte). Dòng log "FWD ..." giờ chỉ hiển thị 4 byte đầu của mỗi khung thay vì 8 (vẫn đủ thấy địa chỉ slave + mã hàm để debug bằng mắt). Ngân sách RAM hiện đang ở mức rất sát (0 byte dư) — nếu sau này cần thêm tính năng mới, nhiều khả năng phải cắt giảm chỗ khác tương ứng.

**Vẫn cần bạn build lại (`pio run`) để xác nhận hết lỗi RAM trước khi nạp xuống board.**

## 8. Retest sau khi nạp fix bước 2 + tăng `rf redund 6` (19:17, 07/07/2026) — phát hiện bug hiển thị log tự gây ra

**Nguồn dữ liệu:** `docs/File_Log_19-07-07-2026/fwd_log_20260707_191739.xlsx` + `ModbusLog_20260707_191734.xlsx`. Thời lượng ~6.9 phút.

Log lần này gần như 100% frame bị gắn nhãn "(>16B)" (4629/4670 dòng), thoạt nhìn như thảm họa. Nhưng kiểm tra kỹ:

- **Độ dài frame thực tế trong log vẫn đúng chuẩn: 7-8 byte** (phân bố độ dài: 8 byte × 2474 lần, 7 byte × 2150 lần) — KHÔNG có dấu hiệu ghép khung thật.
- **CRC lỗi = 0** trong suốt phiên (delta CRC lỗi ở "Link 5s" = 0).
- `ModbusLog` (đọc trực tiếp giá trị nhiệt độ/độ ẩm, không liên quan tới log debug) cho thấy lỗi ứng dụng 14/139 (~10.1%) — cùng mức với các lần đo trước, không phải hỏng hoàn toàn.

**Nguyên nhân:** ở lần vá RAM ngay phía trên (lần 3), mình giảm phần xem trước (preview) của dòng log "FWD ..." từ 8 xuống 4 byte để tiết kiệm RAM. App giám sát dùng dấu "..." (xuất hiện khi preview bị cắt ngắn) làm dấu hiệu nghi ngờ khung bị ghép/quá dài. Vì khung Modbus thường dài 7-8 byte, với preview chỉ 4 byte thì HẦU HẾT khung hợp lệ đều bị cắt và gắn nhầm nhãn "(>16B)", dù bản thân khung hoàn toàn đúng. Đây là bug hiển thị/phân loại của log debug do chính lần vá RAM lần 3 gây ra — không phải lỗi giao tiếp thật.

**Đã sửa (lần 4):** đổi hàng đợi log từ "2 chỗ trống × 4 byte preview" sang **1 chỗ trống × 8 byte preview** (dùng 1 cờ bool đơn giản thay vì head/tail — với 1 chỗ trống duy nhất thì kiểu so sánh head/tail cũ luôn báo "đầy" sai). Cách này vừa khôi phục preview đủ 8 byte (hết bug hiển thị), vừa **tốn RAM ít hơn** phương án cũ (~13 byte so với ~18 byte trước đó).

File thay đổi: `src/drivers/bridge.cpp` (chỉ file này).

**Việc cần làm tiếp:**
1. Build lại (`pio run`) — thay đổi lần này giảm RAM so với trước, không phát sinh lỗi mới.
2. Nạp lại cả 2 board, chạy lại test ≥20-30 phút.
3. Lúc đọc kết quả: nhìn cột "Kiểm tra" phải thấy phần lớn là "OK" trở lại (không còn full "(>16B)"); vẫn theo dõi đúng 2 chỉ số quyết định như mục 6/7: tần suất merge thật (frame dài 12-16 byte) và CRC lỗi/frame ngắn phía RF.

## 9. Test qua đêm ~12.4 giờ (19:46 07/07 → 08:12 08/07/2026) — fix bước 2 CHƯA đủ, đã tìm ra và vá lỗ hổng còn sót

**Nguồn dữ liệu:** `docs/File_Log_07_08-07-2026/fwd_log_20260707_194626.xlsx` + `ModbusLog_20260707_194635.xlsx`. Thời lượng ~12.44 giờ — mẫu lớn, đáng tin cậy nhất từ trước tới nay.

**Xác nhận bug hiển thị (mục 8) đã hết:** chỉ 518/331.226 dòng bị gắn "(>16B)" (0.16%), so với gần 100% trước đó — đúng như kỳ vọng sau fix lần 4.

**Phát hiện quan trọng:** tần suất ghép khung thật (frame RS485→RF dài 9-16 byte, tất cả đều đúng mẫu "mảnh vụn cũ dính liền lệnh mới", ví dụ `00 6B 00 01 F5 D6 01 03 ...`) = **518 lần / 12.44 giờ ≈ 41.6 lần/giờ** — GẦN NHƯ KHÔNG ĐỔI so với baseline gốc trước bất kỳ fix nào (39.9 lần/giờ) và so với lần đo 30 phút sau fix `rs485_send()` (39.7 lần/giờ). Nghĩa là **fix bước 2 (`rs485_send()` không-block) không thực sự giải quyết được vấn đề**, dù hướng chẩn đoán (do `flush()` block CPU) là đúng.

**Nguyên nhân còn sót (đã tìm ra):** fix bước 2 loại bỏ được việc BLOCK CPU của `flush()`, nhưng lại khiến `bridge_rs485_step()` **chủ động bỏ qua hoàn toàn việc đọc RS485 RX trong suốt lúc đang tự gửi** (`rs485_send_pending()==true`) — tạo ra đúng 1 "cửa sổ mù" có độ dài y hệt như trước (chỉ khác cơ chế: chủ động bỏ qua thay vì bị block). Nếu ngay lúc bắt đầu tự gửi đã có sẵn 1 phần lệnh đang gom dở (`s_len>0`, chưa đủ khoảng lặng 1.75ms để flush), phần đó bị "đóng băng" xuyên suốt cửa sổ mù này, rồi dính liền với lệnh thật sự đầu tiên ngay sau khi gửi xong — đúng khớp mẫu lỗi.

**Đã sửa (lần 5):** khi `rs485_send_poll()` báo vừa hoàn tất 1 lần gửi, chủ động:
- Xả sạch mọi byte "echo"/rác có thể đã lọt vào RX trong lúc gửi (một số module RS485 echo lại chính dữ liệu vừa phát khi DE đang bật).
- Xóa bỏ phần đang gom dở trước đó (`s_len=0`) — phần này không bao giờ hoàn chỉnh được nữa vì bridge vừa chiếm bus.
- Đặt lại mốc `s_last_byte_us` cho sạch.

Nhờ vậy, lệnh đầu tiên được gom SAU thời điểm này luôn là 1 khung MỚI, không còn dính với bất kỳ gì trước đó. Chỉ file `src/drivers/bridge.cpp` thay đổi, không thêm biến RAM mới (tái dùng `s_len`/`s_last_byte_us` sẵn có) nên không có rủi ro RAM lần này.

**Việc cần làm tiếp:**
1. Build lại, nạp cả 2 board, chạy test dài (khuyến nghị lại qua đêm để so sánh công bằng với mẫu 12.4 giờ này).
2. Chỉ tiêu thành công: tần suất ghép khung phải giảm mạnh từ mức ~41.6 lần/giờ hiện tại (lý tưởng gần 0). Nếu vẫn không đổi, cần tiếp tục điều tra sâu hơn (có thể còn 1 nguồn "cửa sổ mù" khác chưa phát hiện).

## 10. Bổ sung công cụ chẩn đoán RF (2026-07-08): lệnh CLI `rf scan`

Trong lúc chờ verify các fix RS485 ở trên, đã bổ sung thêm 1 công cụ hỗ trợ hướng tối ưu #3 (kiểm tra vật lý/nhiễu kênh RF) đã đề xuất trước đó — **an toàn tuyệt đối với các fix đang chờ test** vì không đụng tới `bridge.cpp`/`rs485.cpp`/RTOS, và không thêm biến RAM tĩnh nào (chỉ dùng biến cục bộ trên stack khi chạy lệnh).

**Lệnh mới:** `rf scan` — quét toàn bộ 126 kênh nRF24 (0-125), đo mức nhiễu bằng carrier detect (RPD) của chip nRF24L01+ trên từng kênh (~1.6 giây), rồi báo ra 8 kênh ít nhiễu nhất kèm mức nhiễu kênh đang dùng để so sánh. Lệnh này làm gián đoạn liên lạc RF bình thường ~1-2 giây trong lúc quét (phải đổi kênh liên tục) nên chỉ nên chạy thủ công lúc khảo sát lắp đặt, không tự động chạy trong vận hành.

File thay đổi: `src/drivers/rf_link.cpp`, `src/drivers/rf_link.h`, `src/hal.cpp` (thêm lệnh + dòng help).

**Cách dùng:** sau khi build/nạp firmware mới, gõ `rf scan` qua console debug (baud 115200), xem kênh nào có mức nhiễu thấp nhất tại vị trí lắp đặt thật, rồi đổi bằng `rf ch <n>` nếu kênh hiện tại (mặc định 120) không phải là tốt nhất.

**Đã cân nhắc nhưng KHÔNG làm (hướng #2 - redundancy riêng theo từng thiết bị):** ý tưởng dùng loss‰ theo từng `dev_id` đã có sẵn (`rf_dev_loss_permille()`) để tự tăng redundancy riêng cho thiết bị đang rớt nặng. Sau khi đọc lại `dipsw.h`, phát hiện `dev_id` (từ DIP switch) được thiết kế **CỐ Ý ĐỘC LẬP** với địa chỉ Modbus thật của từng slave (comment gốc: "DOC LAP HOAN TOAN voi dia chi Modbus that... ky thuat vien tu ghi lai 2 con so rieng"). Vì bridge chỉ thấy được địa chỉ Modbus (byte đầu khung RTU) chứ không biết `dev_id` tương ứng, việc suy luận "khung này gửi cho dev_id nào" từ địa chỉ Modbus là **không có cơ sở chắc chắn** — nếu làm liều sẽ tra nhầm dữ liệu loss của thiết bị khác, hoặc feature không có tác dụng gì (im lặng), tùy may rủi theo cách kỹ thuật viên đặt DIP thực tế. Cần có thêm 1 bảng ánh xạ "địa chỉ Modbus ↔ dev_id" (cấu hình qua CLI, lưu Flash) mới làm được đúng - đây là việc lớn hơn, cần bạn xác nhận trước khi làm, không tự ý triển khai.

## 11. Retest sau fix lần 5 (10:06 → 11:12, 08/07/2026) — xác nhận fix ghép khung có tác dụng, lộ rõ nút thắt cổ chai thật sự nằm ở RF

**Nguồn dữ liệu:** `docs/File_Log_11h_08-07-2026/fwd_log_20260708_100632.xlsx` + `ModbusLog_20260708_100641.xlsx`. Thời lượng ~65.6 phút (3.935.75s) — test THẬT ĐẦU TIÊN sau khi nạp fix lần 5 (mục 9).

### 11.1. Ghép khung RS485 — GIẢM RÕ RỆT, đúng như kỳ vọng

| Chỉ số | Trước fix lần 5 (12.4h) | Sau fix lần 5 (65.6 phút) |
|---|---|---|
| Tần suất ghép khung (frame 9-16 byte) | ~41.6 lần/giờ | **~14.6 lần/giờ (16 lần)** |
| Mức giảm | — | **~65%** |
| CRC lỗi | — | 20 / 20.836 dòng (~0.1%) |
| Frame ngắn (1-4B) | — | 40 / 20.836 dòng (~0.19%) |

16 khung ghép vẫn giữ đúng mẫu lỗi cũ (mảnh vụn lệnh trước dính đầu lệnh sau, dài 11-14 byte, ví dụ `00 69 00 01 54 16 01 03 ...`) — xác nhận đây là ghép thật, không phải bug hiển thị (bug mục 8 đã sửa từ fix lần 4, preview vẫn đủ 8 byte).

**Kết luận mục này:** fix lần 5 CÓ tác dụng thật, giảm ghép khung khoảng 2/3, nhưng CHƯA về 0 — vẫn còn ~14-15 lần/giờ. Có thể còn 1 cơ chế "cửa sổ mù" nhỏ hơn chưa bắt hết (ví dụ trễ giữa lúc `rs485_send_poll()` báo xong và vòng lặp kế tiếp thực sự đọc RX), nhưng mức độ ảnh hưởng giờ đã nhỏ (dưới 1% tổng số khung).

### 11.2. Phát hiện quan trọng hơn: "Lỗi tổng %" KHÔNG giảm theo — nút thắt cổ chai thật nằm ở tầng RF, không phải RS485

| Chỉ số | Baseline gốc (88.7 phút) | Sau fix lần 5 (65.6 phút) |
|---|---|---|
| Lỗi tổng % (trung bình cửa sổ 5s) | 4.68% | **13.40%** |
| Dải lỗi tổng % | 2.86% – 8.23% | 10.83% – 14.94% |
| Lỗi ứng dụng (ModbusLog "LỖI") | 127/1.666 (~7.6%) | **114/946 (~12.05%)** |
| Tổng lỗi khung raw (CRC+ngắn+ghép, cả 2 chiều) | ~742 (~2%) | 76 / 20.836 (~0.36%) |

Điểm mấu chốt: tổng lỗi ở CẤP KHUNG RAW (CRC lỗi + frame ngắn + ghép, cột "Kiểm tra" trong FWD log) chỉ chiếm **0.36%** — rất thấp, và các cơ chế đã sửa (log block, `flush()` block, cửa sổ mù RX) đều đã có hiệu quả rõ. Nhưng chỉ số "Lỗi tổng %" (= (RS485→RF − RF→RS485) / RS485→RF, tức tỷ lệ LỆNH GỬI ĐI MÀ KHÔNG CÓ PHẢN HỒI QUAY VỀ qua bridge) lại **cao hơn gần 3 lần** so với baseline gốc, và khớp sát với tỷ lệ lỗi ứng dụng thực đo (12.05% ở ModbusLog).

Đã kiểm tra "Hiệu số" (RS485→RF − RF→RS485) tăng dần ĐỀU trong suốt 65 phút (chỉ 1 cửa sổ 5s có bước nhảy bất thường >5, còn lại đều <5/cửa sổ) — nghĩa là đây là hiện tượng **dàn trải liên tục**, không phải 1 sự cố tức thời (mất điện, board treo...).

**Diễn giải:** vì lỗi khung RAW đã giảm xuống rất thấp (0.36%) mà tỷ lệ "gửi đi không có phản hồi" vẫn ~13-14%, phần chênh lệch này KHÔNG còn do lỗi tách khung RS485 nữa — mà do **gói tin bị rớt hoàn toàn trên chặng RF** (gửi redundant "mù" không ACK, xem mục 5/6 báo cáo gốc): 3 bản sao gửi liên tiếp vẫn không đủ để chắc chắn ít nhất 1 bản tới nơi, do nhiễu kênh (kênh 120 nghi có traffic ngoài — xem mục 10, `rf scan`) hoặc do khoảng cách/công suất tại các vị trí lắp thực tế. Đây đúng là hiện tượng bạn từng nêu ở `rf devices`: dev_id có loss‰ heartbeat = 0% (heartbeat có redundancy riêng và tần suất thấp, dễ lọt) nhưng vẫn rớt frame dữ liệu thật (tần suất cao hơn, cạnh tranh băng thông kênh nhiều hơn).

**Ghi chú phụ (không phải lỗi mới):** cột `HB_TX` trong "Link 5s" tăng chỉ 428 trong 3.935s (~0.11/s), thấp hơn nhiều so với tốc độ thiết kế (2 lần/giây). Đây gần như chắc chắn do bộ đếm `HB_TX`/`HB_RX` bị reset giữa chừng (lệnh `rf reset` — công cụ đã hướng dẫn dùng ở mục kiểm tra `rf devices` trước đó), khiến phép tính "hiệu số toàn phiên" bị lệch mẫu số thời gian. Không ảnh hưởng tới các chỉ số ghép khung/lỗi tổng ở trên (2 hệ đếm độc lập nhau — "Link 5s" cột RS485→RF/RF→RS485/CRC/Ngắn do app giám sát tự đếm dòng log, không liên quan lệnh `rf reset` phía firmware).

### 11.3. Kết luận và hướng tiếp theo

1. **Fix lần 5 (ghép khung RS485) xác nhận có tác dụng** — nên giữ nguyên, không cần sửa thêm ở bridge/rs485.cpp trừ khi muốn triệt để 100% (hiện đã dưới 1%, lợi ích cận biên thấp so với rủi ro RAM/thời gian).
2. **Ưu tiên tiếp theo nên chuyển sang tầng RF** — đây mới là nguồn lỗi lớn nhất hiện tại (~13-14%, gấp ~35 lần lỗi khung RS485). Hai hướng đã đề xuất trước đó, giờ có dữ liệu củng cố rõ ràng để làm:
   - Chạy `rf scan` tại đúng vị trí lắp đặt thật, đổi kênh (`rf ch <n>`) khỏi kênh 120 nếu có kênh sạch hơn.
   - Cân nhắc tăng `rf redund` (hiện đang cố định 3 suốt phiên, do RAM-only reset về mặc định sau nạp lại) lên 4-6 nếu kênh vẫn còn nhiễu sau khi đổi, đổi lấy độ trễ/băng thông.
3. Chưa cần làm: redundancy riêng theo dev_id, mở rộng 32 thiết bị — cả 2 việc này đã bàn nhưng chưa có yêu cầu triển khai/test cụ thể.

## 12. Đã triển khai (2026-07-08): redundancy tự thích ứng theo lỗi thực đo — CHỈ TRÊN HUB

**Hướng #2** trong mục 11.3 — thay vì `rf redund` cố định phải chỉnh tay, board tự tăng/giảm theo đúng chỉ số đã chứng minh phản ánh thật mất gói RF (hiệu số RS485→RF/RF→RS485), không dùng heartbeat loss‰ (dễ đánh lừa, xem case dev_id 1 mục trước).

**Vì sao chỉ chạy trên Hub (dev_id=0), không chạy trên Slave:** với Hub, mỗi lệnh RS485→RF là 1 request gửi cho đúng 1 slave, kỳ vọng đúng 1 response RF→RS485 quay về — hiệu số = mất gói thật. Nhưng trên 1 Slave, mạng là broadcast (mọi slave nhận HẾT mọi request của toàn mạng, không riêng địa chỉ của nó) nên RF→RS485 (request nhận được) luôn lớn hơn nhiều RS485→RF (chỉ request đúng địa chỉ thiết bị nó mới có response) — hiệu số trên slave gần như luôn cao nhưng KHÔNG phải mất gói, chỉ là "không phải request cho tôi". Chạy thuật toán này trên slave sẽ đẩy redundancy lên MAX vô ích cho mọi slave, còn gây nghẽn kênh ngược lại — đúng kiểu lỗi mà thuật toán auto-redund CŨ đã bị bỏ (ghi chú 2026-07-06 trong `rf_link.h`), chỉ khác chiều: cũ "luôn thấy tốt", đây "luôn thấy xấu giả tạo".

**Cách hoạt động:** kiểm tra mỗi vòng lặp task RF, nhưng chỉ THỰC SỰ đánh giá khi đã tích lũy đủ ≥20 khung RS485→RF mới kể từ lần kiểm tra trước (kích hoạt theo SỐ MẪU, không theo đồng hồ cố định 5 giây — xem lý do RAM bên dưới); nhờ vậy phản ứng nhanh khi traffic cao, chờ gom đủ mẫu khi traffic thấp. Hiệu số >8% → tăng redund thêm 1 (tối đa `RF_REDUNDANT_TX_MAX`=6). Hiệu số <2% → giảm redund bớt 1, nhưng KHÔNG giảm dưới `RF_REDUNDANT_TX_DEFAULT`=3 (mức đã kiểm chứng qua bench test) — chỉ tăng vượt mặc định khi cần, không tự ý xuống thấp hơn mức đã xác nhận an toàn.

**Phạm vi/hạn chế đã biết:** chỉ tự điều chỉnh độ dự phòng CHIỀU Hub→Slave (redundancy gửi đi của Hub). Chiều Slave→Hub (response) vẫn dùng giá trị cố định người dùng tự set qua `rf redund` trên từng slave — thuật toán này KHÔNG giải quyết được phần mất gói ở chiều đó. Nếu sau khi test vẫn còn "Lỗi tổng %" cao, cần xem xét tăng tay `rf redund` trên các slave hay rớt (đối chiếu qua `rf devices`/loss‰, dù chỉ số này cũng có hạn chế riêng).

**Đã vá RAM tràn 8 byte:** bản đầu dùng 3 biến `uint32_t` (mốc thời gian + 2 snapshot đếm = 12 byte) — build thật báo `region 'RAM' overflowed by 8 bytes`. Đã đổi sang kích hoạt theo số mẫu (bỏ hẳn biến mốc thời gian, 2 snapshot còn lại đổi qua `uint16_t`) — chỉ còn 4 byte RAM mới, giảm đúng 8 byte cần thiết.

File thay đổi: `src/drivers/bridge.cpp` (thêm `adapt_redundancy_check()`, gọi từ `bridge_rf_step()`), `src/drivers/bridge.h` (comment).

**Việc cần làm tiếp:**
1. Build lại (`pio run`) — xác nhận hết lỗi RAM trước khi nạp.
2. Nạp cả 2 board (Hub + Slave, cùng 1 file build).
3. Chạy lại đúng kịch bản test, theo dõi ô "Dự phòng" trên app: nếu điều kiện RF kém, số này trên board Hub sẽ tự tăng dần (kèm dòng log "BRIDGE: adaptive redund TANG len ..." trên console debug); so sánh "Lỗi tổng %" trước/sau để xem redundancy tự tăng có kéo giảm được tỷ lệ mất gói hay không.
