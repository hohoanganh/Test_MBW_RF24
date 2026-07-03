# MBW RF24 RS485 2.0 – Quy trình test (2 board, 2 máy tính riêng)

Chức năng **mặc định** của board là tự động forward RS485 ⇄ Wireless ngay khi
cấp nguồn (bridge luôn BẬT, không cần lệnh kích hoạt). App test **chỉ kết nối
1 board / 1 máy tính** — khi cần kiểm chứng forward giữa 2 board, dùng **2 máy
tính riêng biệt, mỗi máy chạy 1 instance app**, mỗi bên tự quan sát board của
mình qua tab **"Giám sát Forward"**. Xem kiến trúc hệ thống:
`docs/MBW_RF24_RS485_2I0_RevB_Hardware.md`.

## Chuẩn bị

1. Nạp firmware (`pio run -t upload`) cho cả 2 board.
2. Cấp nguồn 8–28VDC cho cả 2 board qua J3 — board sẽ **tự bật bridge ngay**
   (console in `BRIDGE: ON (mac dinh, forward RS485 <-> Wireless)`).
3. Mỗi board cắm cáp Console (RJ45/USB-RS232 hoặc header debug J9) vào **1
   máy tính riêng**, chạy `python mbw_test_app.py` trên từng máy.
4. **Đặt DIP switch giống nhau trên cả 2 board**:
   - SW1-6 (Network ID): CÙNG giá trị trên A và B.
   - SW7-8 (Baudrate RS485): CÙNG giá trị trên A và B.
   - Xác nhận bằng lệnh `dip` (tab Terminal) hoặc bước "Đọc DIP switch" trong
     Test tự động — mỗi máy tự đọc board của mình, kỹ thuật viên 2 bên tự đối
     chiếu qua điện thoại/lời nói vì app không thấy được máy kia.

## Test tự động (chạy trên TỪNG máy, độc lập)

```bash
python mbw_test_app.py
```

Kết nối COM, tab **"Test tự động"**, bấm **▶ Run Test**. 6 bước, tất cả chạy
trên board đang nối với MÁY NÀY (không liên quan board kia):

| # | Bước | Cần chuẩn bị thêm |
|---|---|---|
| 1 | Xác thực ID | — |
| 2 | Đọc DIP switch | Đọc NETID/BAUD của board này; đối chiếu thủ công với máy kia |
| 3 | SPI Flash (JEDEC ID) | — |
| 4 | RTC PCF85063 | — |
| 5 | RS485 loopback | Nối tắt chân A-B trên đầu nối RJ11 của board này (không ảnh hưởng bridge — lệnh `rsl` chạy đồng bộ) |
| 6 | nRF24L01 có mặt (SPI) | — |

Báo cáo ghi vào `mbw_test_report.xlsx` trên MÁY ĐANG CHẠY app đó (mỗi máy có
file report riêng, không gộp 2 board vào 1 hàng như thiết kế cũ).

## Giám sát Forward (kiểm chứng chức năng chính, cần 2 máy cùng lúc)

Đây là cách xác nhận **đúng chức năng thật của sản phẩm**: dữ liệu vào cổng
RS485 vật lý của 1 board phải ra được cổng RS485 vật lý của board kia qua sóng
không dây, hoàn toàn tự động (bridge mặc định luôn bật, không cần bật/tắt gì).

**Chuẩn bị:** đấu dây RS485 A-A, B-B (twisted pair) giữa đầu nối RJ11 của
Board A và Board B nếu muốn test bằng cách bơm dữ liệu từ 1 board (xem cách 2
bên dưới); nếu đã có Modbus Master/Slave thật nối vào từng board thì không
cần bước này — cứ để bridge chạy nền và quan sát tab Forward là đủ.

### Cách 1 — Có Modbus Master/Slave thật

Không cần thao tác gì thêm: cắm Modbus Master vào RS485 của Board A, Modbus
Slave vào RS485 của Board B (hoặc ngược lại). Mỗi khi có giao tiếp Modbus,
tab **"Giám sát Forward"** trên máy A sẽ hiện dòng `RS485 → RF`, và trên máy B
hiện dòng `RF → RS485` (và ngược lại khi Slave trả lời) — 2 kỹ thuật viên tự
đối chiếu số liệu giữa 2 màn hình.

### Cách 1b — Dùng công cụ Modbus Poll Test tích hợp (không cần Modbus Slave thật riêng)

Nếu chưa có sẵn thiết bị Modbus Slave thật, dùng chính công cụ tích hợp trong
app: tab **"Giám sát Forward"** → nút **"🔌 Modbus Poll Test (RS485 thật)"** mở
`modbus_poll_app.py` (Modbus RTU Master thật FC3/FC6, không đi qua console CLI
mà đi thẳng qua **cổng RS485 vật lý** của board). Cách làm:

1. Trên máy A: bấm nút này, cửa sổ Modbus Poll Test mở ra — chọn **cổng COM
   nối vào đầu RS485 vật lý của Board A** (khác cổng console CLI đang mở ở
   cửa sổ chính), chọn baudrate/parity/slave ID phù hợp thiết bị Modbus Slave
   thật đang nối ở đầu Board B, bấm **Connect** rồi theo dõi bảng dữ liệu/biểu
   đồ đọc được theo chu kỳ poll.
2. Trên máy B: đầu RS485 vật lý của Board B nối vào 1 thiết bị Modbus Slave
   thật (cảm biến, PLC...). Có thể dùng nút **Auto Scan** trong cửa sổ Modbus
   Poll Test (máy A) để tự dò baudrate + slave ID nếu chưa biết.
3. Theo dõi đồng thời tab **"Giám sát Forward"** trên cả 2 máy: mỗi lần Modbus
   Poll (máy A) gửi request/nhận response, dòng `RS485 → RF` xuất hiện trên
   máy A và `RF → RS485` xuất hiện trên máy B (và ngược lại) — xác nhận toàn
   bộ chuỗi Modbus RTU thật đã đi trọn vẹn qua cầu không dây.
4. Có thể bật ghi log Excel (tab "Ghi log Excel" trong cửa sổ Modbus Poll
   Test) để lưu lại dữ liệu đọc được + toàn bộ frame TX/RX phục vụ đối chiếu
   sau test.

Đây là công cụ + tab riêng biệt (chạy trong cửa sổ/tiến trình khác), có thể mở
song song với app test chính mà không ảnh hưởng cổng console CLI đang kết nối.

### Cách 2 — Chưa có Modbus thật, tự bơm dữ liệu để test

Trên máy đang nối Board A, tab **"Giám sát Forward"** → ô **"Gửi thử lên
RS485 của board này"** → nhập chuỗi bất kỳ (vd `MBW TEST 001`) → bấm **Gửi**.
App gửi lệnh CLI `rs485 <text>` khiến Board A phát chuỗi đó lên bus RS485
vật lý của chính nó. Nếu đã đấu dây RS485 thật giữa A và B:

1. Máy A: tab Forward hiện dòng `RS485 → RF` (Board A vừa đóng gói dữ liệu
   RS485 vừa nhận/phát và relay qua RF).
2. Máy B: tab Forward hiện dòng `RF → RS485` với cùng nội dung/preview hex —
   xác nhận Board B đã nhận qua sóng và đẩy ra RS485 vật lý thành công.

Lặp lại theo chiều ngược (gửi thử từ Board B) để xác nhận cả 2 chiều.

> Lưu ý: khi 2 board đấu chung bus RS485 (twisted pair), board đang tự phát
> `rs485 <text>` cũng chính là node lắng nghe RS485 của chính nó (bridge của
> nó cũng đang chạy) — nhưng vì đang trong lúc chính nó transmit (DE bật) nên
> không tự thu lại dữ liệu mình vừa phát (xem `RS485_DIR` trong
> `docs/MBW_RF24_RS485_2I0_RevB_Hardware.md`), nên sẽ không xuất hiện dòng
> `RS485 → RF` giả từ chính board đó nhận lại tiếng vọng của mình.

### Theo dõi chất lượng RF Link (heartbeat, giống telemetry)

Mỗi board tự phát 1 khung heartbeat mỗi giây, độc lập với dữ liệu Modbus. Tab
Forward hiển thị nhãn **RF Link** (UP/DOWN theo thời gian thực) — nếu quá 3
giây không nhận được gì từ board bên kia thì tự chuyển sang **DOWN** (đỏ),
ngay cả khi chưa từng có Modbus chạy qua. Đây là cách xác nhận nhanh 2 board
vẫn "thấy" nhau trước khi test dữ liệu, tương tự cách Mission Planner báo mất
kết nối MAVLink với thiết bị bay.

Bấm **"Đọc RF Link (rf stat)"** để xem chi tiết: peer id, % mất ước lượng,
độ dự phòng gửi lặp hiện tại (2-6, tự tăng khi link kém/tự giảm khi ổn định),
số heartbeat tx/rx. Nếu di chuyển 2 board ra xa nhau hoặc che chắn để test
biên độ phủ sóng, quan sát nhãn này và log console (`RF: link kém, tăng độ dự
phòng lên N`) để biết board đang tự bù trừ nhiễu/khoảng cách ra sao trước khi
dữ liệu Modbus thật sự bị rớt.

### Điều khiển bridge khi cần tạm dừng (chẩn đoán/hiệu chỉnh)

Tab Forward có nút **Bridge: BẬT/TẮT** và **Log: BẬT/TẮT** (gửi lệnh CLI
`bridge on|off` / `bridge log on|off`) — dùng khi kỹ thuật viên cần tạm ngắt
forward để chạy các lệnh RS485/RF thủ công khác qua tab Terminal mà không bị
bridge tranh đọc UART. Nhớ bật lại `bridge on` sau khi test xong để board trở
về hành vi mặc định.

## Test qua Terminal (thủ công, 1 board)

Tab **Terminal** có nút lệnh nhanh (`id, ver, dip, flash, rtc, rf id, rf stat,
bridge stat, bridge log on/off`) và ô gõ lệnh tự do — dùng khi cần chẩn đoán
sâu hơn (ví dụ `rf ch 122` để đổi kênh RF thử trong môi trường nhiều nhiễu,
rồi quay lại tab Forward để xem forward có còn hoạt động ổn định không).
