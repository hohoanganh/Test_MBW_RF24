# OPMS 1.6 Slave Board – Quy trình Test chức năng

Tài liệu mô tả trình tự test 12 nhóm chức năng bằng app `opms_test_app.py` + firmware test.
Mỗi nhóm: đấu nối → thao tác → lệnh CLI app gửi → tiêu chí PASS/FAIL.
Tham chiếu: `docs/OPMS_1.6_Hardware.md`, `docs/OPMS_App_TestFlow.md`, `app_config.json`.

## Chuẩn bị chung

1. Nạp **firmware test** (v0.3.0): `pio run -t upload`. Đèn **LIFE (PE7) nháy** = OK.
2. Cấp **nguồn cho board** (12V hoặc 48V theo khối cần test).
3. Cắm cáp **RS232 USB → cổng Console slave**. Mở app → chọn **COM** → **Kết nối**
   (chấm xanh + hiện `OPMS_1.6_SLAVE` = đúng thiết bị).
4. Test từng thẻ thủ công, hoặc bấm **▶ Chạy tất cả (Auto)** để chạy tuần tự + chấm PASS/FAIL.

Quy ước kết quả: **PASS** (xanh) = trong ngưỡng · **FAIL** (đỏ) = ngoài ngưỡng / không phản hồi ·
**MANUAL** = cần đo tay (Volt kế) rồi tự xác nhận.

---

## Bảng kịch bản

| #   | Nhóm                       | Đấu nối / Chuẩn bị                                   | Thao tác                   | Lệnh app gửi                       | Tiêu chí PASS                                               |
| --- | -------------------------- | ---------------------------------------------------- | -------------------------- | ---------------------------------- | ----------------------------------------------------------- |
| 1   | **Nhiệt độ** (NTC1–4)      | B1: chưa cắm cảm biến. B2: cắm 4 NTC 10K             | Đọc 4 kênh                 | `ntc 1..4`                         | Không cắm: ~**255** cả 4. Cắm: nằm trong dải (calib ~25–29) |
| 2   | **Nguồn 5V độ ẩm** (H1,H2) | Volt kế đo tại cổng H1/H2                            | Bật/tắt 5V từng cổng       | `humpwr <1-2> on/off`              | Bật **≈5V**, tắt **≈0V** (đo tay → MANUAL)                  |
| 3   | **Độ ẩm** (H1,H2)          | B1: chưa cắm. B2: cắm 2 cảm biến RHT (bật 5V trước)  | Đọc 2 kênh                 | `humpwr on` → `hum 1..2`           | Không cắm: ~**255**. Cắm: dải (calib ~45–65)                |
| 4   | **Đầu vào** (I1–I8)        | B1: không cắm cổng nào. B2: cắm lần lượt I1→I8 (48V) | Đọc 8 GPI                  | `gpi`                              | Không cắm: mọi bit = **1** (GPI=255). Cắm In: bit n = **0** |
| 5   | **Đầu ra** (O1–O4)         | **Tắt tất cả trước.** Mắc tải vào O1–O4              | Bật/tắt từng cổng, đo dòng | `gpo <1-4> on/off` → `gpoi`        | Bật: **80–320 mA**. Tắt: **~0 mA**                          |
| 6   | **Quạt 48V** (Fan1–4)      | Cắm 4 quạt 48V                                       | Bật/tắt + đọc tốc độ       | `fan <1-4> on/off` → `fanr <n>`    | Bật: **45–65 rps** (calib xung). Tắt: **0**                 |
| 7   | **Relay AC**               | Mắc tải AC + đo dòng                                 | Bật/tắt relay              | `ac on/off` → `aci`                | Bật: **140–260 mA**. Tắt: **0–50 mA**                       |
| 8   | **Quạt nhỏ** (DEV fan)     | Cắm quạt nhỏ thiết bị                                | Bật/tắt + đọc feedback     | `dfan <1-3> on/off` → `dfanfb <n>` | Bật: feedback hợp lệ (calib cực tính). Tắt: 0               |
| 9   | **Nguồn 12V RS485**        | Volt kế đo tại cổng RS485                            | Bật/tắt nguồn              | `rspwr on/off` → `rspg`            | Bật: **PG=1** (đo **≈12V**). Tắt: **PG=0**                  |
| 10  | **Cổng RS485** (A1B1→A4B4) | Cắm thiết bị **Modbus** (hoặc dây loopback) lần lượt | Probe từng cổng            | `rs485 <1-4>`                      | Có byte phản hồi (`RS485n_RX > 0`)                          |
| 11  | **Flash & RTC**            | Không cần đấu                                        | Đọc ID flash               | `flash`                            | `FLASH=OK` (ID Winbond 0xEF)                                |
| 12  | **Feedback**               | Không cần đấu                                        | Đọc chân feedback          | `dfanfb 1`                         | Đọc được giá trị (không lỗi giao tiếp)                      |

---

## Hướng dẫn đấu nhanh (theo hướng dẫn OPMS cũ)

- **Bước 1,2,3:** đấu cảm biến nhiệt/ẩm theo sơ đồ đấu nối OPMS 1.6.
- **Bước 4:** cắm lần lượt I1→I8 (từng cổng đơn lẻ), 1 ↔ 51V, 0 ↔ 0V.
- **Bước 5,6,7,8:** đấu tải theo sơ đồ. **Lưu ý bước 5: tắt hết ngõ ra trước** để tránh luôn có dòng.
- **Bước 9:** đo áp 12V tại cổng sau khi bật.
- **Bước 10:** dùng thiết bị Modbus được cấp, cắm lần lượt A1B1→A4B4 xem cổng có phản hồi.

---

## Trạng thái hiện tại & điểm cần hiệu chỉnh (calib trên bàn test)

| Hạng mục | Trạng thái | Cần làm |
|---|---|---|
| GPO / Fan / GPI / AC / DEVfan / Flash | ✅ | Calib hệ số dòng (ACS712, AC), xung/vòng quạt, thứ tự bit GPI, cực tính feedback |
| NTC / Độ ẩm | ✅ | Map thang ADC→giá trị đúng dải 25–29 / 45–65 |
| Nguồn 5V/12V | ✅ | 5V đo tay; 12V đọc PG (xác nhận logic PG) |
| Cổng RS485 | ⚠️ (cần jig) | Xác nhận ánh xạ cổng 1–4 ↔ transceiver/DIR; cần thiết bị Modbus |

> Gợi ý ghi nhận: sau mỗi đợt sản xuất, dùng nút **Xuất Excel** trong app để lưu `test_report.xlsx`
> (Nhóm / Bước / Giá trị / Kết quả) kèm số serial board.
