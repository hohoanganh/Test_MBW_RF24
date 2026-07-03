# App Test Modbus RTU (kiểu Modbus Poll) — ghi log Excel

Ứng dụng Python (giao diện Tkinter) để test thiết bị Modbus RTU qua RS485,
tương tự Modbus Poll / Insight Sensor, mặc định cấu hình sẵn theo register
map của cảm biến **ES35-SW** (EPCB — nhiệt độ & độ ẩm SHT35).

## Tính năng

- Kết nối RS485/Modbus RTU qua cổng COM (chọn Port, Baudrate, Parity, Slave ID, Scan rate).
- Tự cài đặt giao thức Modbus RTU (CRC16, FC3 Read Holding Registers, FC6 Write Single Register)
  bằng `pyserial` thuần — không phụ thuộc `pymodbus` nên tránh lỗi lệch version.
- Auto Scan: tự dò baudrate + slave ID (1–15) để tìm thiết bị đang phản hồi.
- Bảng dữ liệu realtime (giống Modbus Poll): Description / Address / Value / Unit / Read-Write.
- Tab **Ghi thanh ghi**: ghi (FC6) các thanh ghi Read/Write — Address ID (100), Baudrate (101),
  Temperature Correction (106), Humidity Correction (107).
- Tab **Biểu đồ**: vẽ Temperature & Humidity theo thời gian thực (matplotlib).
- **Ghi log ra Excel (.xlsx)** theo thời gian thực bằng `openpyxl`:
  - Sheet `AllData`: mỗi dòng = 1 lần poll, đủ tất cả thanh ghi.
  - Sheet riêng cho từng thông số (Temperature, Humidity, …) — giống cách Insight Sensor xuất Excel.
  - Sheet `CommLog`: ghi lại toàn bộ frame TX/RX giao tiếp với thiết bị (giống khung Data Log trong app),
    kèm timestamp chi tiết tới mili-giây. Giới hạn an toàn 50.000 dòng để tránh file phình quá to.
  - Tự động lưu định kỳ (mỗi 5 dòng) để giảm số lần ghi đĩa.
  - **Chống mất dữ liệu khi file bị khóa**: nếu file Excel đang mở trong Excel hoặc bị khóa
    (lỗi Permission denied), app KHÔNG mất dữ liệu — vẫn giữ trong bộ nhớ và tự động thử lưu lại
    ở lần đọc kế tiếp. Bấm "Bắt đầu ghi log" lại (sau khi đóng file Excel) để buộc lưu lại ngay.
  - Nút "Export snapshot Excel" để xuất nhanh 1 bản ghi hiện tại.
- Data Log hiển thị khung TX/RX dạng hex để debug giao tiếp (đồng thời được ghi vào sheet CommLog
  khi đang bật Ghi log Excel).

## Cài đặt

Yêu cầu Python 3.9+ trên Windows. Cài các thư viện:

```bash
pip install -r requirements.txt
```

(`tkinter` đã có sẵn trong Python chuẩn trên Windows, không cần cài thêm.)

## Chạy ứng dụng

```bash
python modbus_pull_app.py
```

## Cách dùng nhanh

1. Cắm bộ chuyển đổi USB-RS485, nối theo sơ đồ: A(+) — A, B(-) — B, GND — GND,
   cấp nguồn 8–30V cho cảm biến ES35-SW.
2. Chọn cổng COM (bấm "Làm mới" nếu chưa thấy), chọn Baudrate mặc định 9600, Slave ID mặc định 1.
3. Bấm **Connect**. Bảng dữ liệu sẽ tự động cập nhật Temperature/Humidity theo chu kỳ Scan rate.
   Nếu chưa biết baudrate/slave ID, bấm **Auto Scan**.
4. Sang tab **Ghi log Excel** → chọn đường dẫn file `.xlsx` → bấm **Bắt đầu ghi log**.
   Dữ liệu sẽ được ghi liên tục vào file cho đến khi bấm **Dừng ghi log** hoặc Disconnect.
5. Sang tab **Ghi thanh ghi** nếu cần đổi Slave ID, Baudrate, hoặc hiệu chỉnh (correction)
   nhiệt độ/độ ẩm. Lưu ý: thiết bị cần re-power (tắt/bật lại nguồn) để áp dụng thay đổi.

## Tùy biến cho thiết bị Modbus RTU khác

Danh sách thanh ghi được định nghĩa ở đầu file `modbus_pull_app.py`, biến `DEFAULT_REGISTER_MAP`.
Mỗi thanh ghi gồm: `name`, `address`, `type` (`uint16`/`int16`/`uint32`/`int32`/`float32`),
`scale` (giá trị thực = raw / scale), `unit`, `rw` (`R` hoặc `RW`). Chỉnh 