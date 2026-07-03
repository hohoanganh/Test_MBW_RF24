# MBW RF24 RS485 2.0 – Quy trình test chất lượng RF Link (v2, 2026-07-03)

> Bản Word chính thức (brand EPCB, dùng cho hồ sơ bàn giao):
> `docs/QuyTrinh_Test_RF_Link_MBW_RF24.docx` — nội dung tương đương file này.

Chức năng mặc định của board: tự forward RS485 ⇄ Wireless ngay khi cấp nguồn
(bridge luôn BẬT). App test kết nối **1 board / 1 máy tính**; test 2 đầu dùng
2 máy, mỗi máy 1 instance `python mbw_test_app.py`.

**Nguyên tắc đánh giá:** nRF24L01 KHÔNG có RSSI (chỉ có bit RPD > -64dBm).
Chất lượng link đo bằng: (1) % mất heartbeat, (2) mức dự phòng REDUND tự
thích ứng, (3) % giao dịch Modbus mất + khung lỗi khi chạy tải thật.

## Chỉ số & nơi xem

| Chỉ số | Nơi xem | Ý nghĩa |
|---|---|---|
| Mất gói heartbeat % | KPI "Mất gói" (tab Giám sát Forward) | LOSS_PROMILLE/10, đo cả khi không tải |
| Dự phòng REDUND | KPI "Dự phòng" | 2-3x khỏe, 4-5x đang gồng, ≥6x kém |
| HB TX/RX | KPI "Heartbeat" | heartbeat 2 chiều còn sống |
| RS485→RF / RF→RS485 + hiệu số | thanh đếm + ô **LỖI** | hiệu số tăng = giao dịch mất; LỖI 60s = chất lượng hiện tại |
| Kiểm tra khung | cột "Kiểm tra" + dòng tổng hợp | CRC16 từng khung: OK / CRC LỖI / EXC xx / NGẮN / QUÁ DÀI (ngưỡng chỉnh được) |
| DROP queue | `bridge stat` / Stress Test | phải luôn = 0 |

## Ngưỡng

| Chỉ số | TỐT | CHẤP NHẬN | KÉM |
|---|---|---|---|
| Mất gói heartbeat | <2% | 2-10% | >10% |
| REDUND | ≤3x | 4-5x | ≥6x |
| LỖI giao dịch (60s) | ≤1% | 1-2% | >2% |
| CRC lỗi / NGẮN / QUÁ DÀI | 0 | — | ≥1 |
| Rớt link (DOWN) | 0 | — | ≥1 |
| DROP queue | 0 | — | ≥1 |

Nghiệm thu: tất cả cột TỐT. Lắp đặt: không có cột KÉM (mức CHẤP NHẬN phải ghi
biên bản). Có cột KÉM → xử lý (mục cuối) rồi test lại.

## 4 luồng test

### T1 — Factory Test (xưởng, từng board, ~2 phút)
- Chuẩn bị: nối tắt A-B RS485 (bước loopback); board mẫu thứ 2 cùng NETID bật
  nguồn trong tầm (2 bước RF); DIP 2 board giống nhau.
- Tab **Factory Test** → ▶ Run Test: 8 bước (id, dip, flash, rtc, rsl,
  rf id, **RF heartbeat**, **RF khung mất ≤100‰**). App tự tắt log FWD khi
  test, xong bật lại. Bước RF khung mất cần link UP ≥30s trước đó.
- Đạt: 8/8 PASS → Export Report (`mbw_test_report.xlsx`). FAIL → không xuất.

### T2 — Bench test (2 board, 30 phút, trước khi mang đi lắp)
- Tab **Stress Test**: 30 phút, mẫu 5s, bơm "rf tx (qua RF)" 200ms / 32 byte.
- App tự chấm ĐẠT/KHÔNG ĐẠT (0 rớt link, LOSS tb ≤50‰, max ≤150‰,
  CRC/FRAG/DROP=0). Xuất CSV lưu hồ sơ.

### T3 — Hiện trường (khi lắp đặt, ≥5 phút/vị trí)
- Tab **Giám sát Forward**, nhìn 5 ô KPI màu xanh/vàng/đỏ.
- Chỉnh vị trí / hướng anten / kênh RF đến khi **ĐÁNH GIÁ LẮP ĐẶT = TỐT** ổn
  định ≥5 phút; ô LỖI (60s) phản ứng sau ~1 phút mỗi thay đổi.
- Chỉ đạt KHÁ sau tối ưu → bàn giao tạm + ghi biên bản.

### T4 — Nghiệm thu (Modbus thật, 10.000 request)
- Sơ đồ: Modbus Poll Test → RS485 board A → RF → board B → Slave thật.
- Cấu hình: timeout 1000ms, delay ≥200ms. Bật **⏺ Ghi Excel** ở CẢ 2 đầu
  (sheet "FWD log" + "Link 5s") + log Excel của Modbus Poll.
- Đạt: lỗi giao dịch <0.1%, CRC=0, DROP=0, 0 lần DOWN. Lưu 3 file log.

## Truy vết lỗi bằng log

1. Sheet **Link 5s**: cột *Hiệu số* nhảy +1 = thời điểm mất giao dịch.
2. Cùng hàng: *Mất gói %* / *Dự phòng* tăng → lỗi RF; vẫn đẹp → nghi slave.
3. Lấy timestamp sang sheet **FWD log** + log Modbus Poll: tìm request không
   có response, khung CRC LỖI / NGẮN tương ứng.

Kịch bản điển hình: TX không có RX → retry → response về TRỄ + mảnh ngắn
(vd `02 00 44`) = vượt timeout master, không phải mất hẳn → tăng timeout
1500ms và/hoặc cải thiện link.

## Xử lý khi không đạt

| Triệu chứng | Hành động |
|---|---|
| Mất gói cao cả khi gần | đổi kênh tránh WiFi 2400-2472MHz (CH≥100); tụ lọc nguồn 3.3V nRF; anten |
| REDUND dao động mãi | nhiễu chu kỳ → đổi kênh / di dời |
| Gần TỐT, xa KÉM | hướng anten, nâng cao, anten ngoài / vị trí trung gian |
| Khung NGẮN, response trễ | tăng timeout Modbus 1500ms + delay poll |
| DROP queue > 0 | tăng scan rate master (poll quá nhanh so với trễ RF) |
| DOWN lặp lại | nguồn sụt khi TX, vượt tầm, trùng NETID hệ khác |

## Lệnh CLI liên quan

`rf stat` (2 dòng đếm gói + link) · `rf tx <text>` · `rf reset` ·
`bridge stat` · `bridge log on|off` · `rtos stat` (stack high-water-mark).

EXC xx trong cột Kiểm tra = slave báo exception Modbus (01 function, 02
address, 03 value, 04 failure, 06 busy) — CRC đúng, **không phải lỗi RF**.
