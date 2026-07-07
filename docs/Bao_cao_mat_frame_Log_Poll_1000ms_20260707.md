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
