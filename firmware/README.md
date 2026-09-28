# Firmware AESL0213C (nRF52811) – chỉ đường

Firmware mở rộng từ [tsl0922/EPD-nRF5](https://github.com/tsl0922/EPD-nRF5) (GPL-3.0, commit `7e31961`)
cho nhãn AESL0213C: nRF52811 + SoftDevice S112 7.3.0, màn 2.13" SSD1680 (122×250, đen/trắng/đỏ).

> Đã nạp OTA thành công trên một nhãn AESL0213C thật (bản 1000). Bản 1001 sửa căn dọc và bật refresh nhanh
> mặc định (bản 1000 đọc nhầm dữ liệu lịch ngủ của firmware gốc thành "tắt refresh nhanh").

## Có gì mới
- **Lệnh chỉ đường `0x40`**: web gửi vài chục byte (icon, khoảng cách, tên đường, còn lại, giờ đến),
  nhãn tự vẽ bằng font tiếng Việt. Các lệnh đến dồn dập được gộp, chỉ vẽ bản mới nhất.
- **Refresh nhanh đen trắng** (thử nghiệm, LUT nạp từ host) cho chỉ đường và đồng hồ, cứ 20 lần thì
  refresh toàn màn một lần để xoá bóng mờ. Tắt/bật và đổi chu kỳ trong tab *Nâng cao* của web (`0x41`).
- Màn **Đồng hồ** và **Lịch** (ngày, thứ tiếng Việt, nhiệt độ, điện áp pin) thay cho lịch âm tiếng Trung.
- Giữ tương thích với web hiện tại: model 2, gói cấu hình báo panel `0x21`, lệnh ghi ảnh `0x30`
  cùng cách đặt cờ (`0x0f` lớp đen, `0xf0` gói tiếp theo), đồng bộ giờ `0x20`, lịch/đồng hồ.
- Vẫn có **nạp qua Bluetooth không cần nút** (Secure DFU) để cập nhật lần sau.

## Hiển thị khi đến gần chỗ rẽ
- Nửa phải: hướng dẫn một dòng ở trên (dài quá thì chạy chữ một lượt khi sang hướng dẫn mới, 2 giây/bước) và **sơ đồ ngã rẽ**
  bên dưới (tuyến đi đậm, nhánh khác mảnh, chấm vị trí chạy về ngã rẽ). Web có thể tắt sơ đồ để hiện chữ nhiều dòng.
- 500 m cuối: thanh tiến trình dưới mũi tên; dưới 50 m: nửa trái đảo màu (chữ trắng nền đen); dưới 20 m: "Ngay!".
- Web gửi thưa khi còn xa (20 s, làm tròn 100 m), dày dần khi gần (dưới 100 m: ~1,5 s, làm tròn 5 m).
- Refresh toàn màn định kỳ (~13 s, để xoá bóng mờ) được hoãn trong 400 m cuối và làm sớm sau khi rẽ nếu phía trước là đoạn thẳng dài.

## Nạp bằng iPhone (OTA)
1. Tải `release/aesl0213c-nav-ota.zip` về iPhone (mở file trên GitHub → *Download*, lưu vào **Tệp**).
2. **Trước khi nạp**: mở web bằng Bluefy, kết nối nhãn, chép lại dòng **Chân (pin)** (để khôi phục nếu cần).
3. Mở app **nRF Connect** → quét → *Connect* `NRF_EPD_xxxx` → biểu tượng **DFU** (góc trên) →
   chọn file zip → *Start*. Giữ iPhone sát nhãn cho tới khi xong (~1 phút). Có thể dùng app
   **nRF Device Firmware Update** thay thế.
4. Nếu app báo lỗi chữ ký/phiên bản (*signature*, *version*, *init packet*): bootloader của nhãn
   dùng khoá khác — nhãn **không bị thay đổi**, vẫn chạy firmware cũ.
5. Sau khi nạp, mở web → kết nối → nhật ký phải có `mtu=… nav=1` và Firmware `v80`.

**Nếu màn không hiển thị** nhưng vẫn kết nối được: gửi lệnh đặt chân trong tab *Nâng cao → Gửi lệnh thô*
`00 mosi sclk cs dc rst busy bs` (6 byte đầu của dòng *Chân (pin)* đã chép, rồi byte thứ 7), sau đó `91` để khởi động lại.

## Build
```sh
./fetch-upstream.sh          # SDK Nordic + công cụ (không đưa vào git)
make                         # cần arm-none-eabi-gcc
make ota                     # cần nrfutil 6.x: pip install --ignore-requires-python nrfutil==6.1.7
```
- Flash ứng dụng: `0x19000`–`0x27000` (sau đó là 2 trang FDS và bootloader ở `0x29000`), hiện dùng ~47,5 KB.
- Gói OTA ký bằng khoá công khai của EPD-nRF5 (`upstream/tools/priv.pem`), `sd-req 0x126`, app version 1000.
- `tools/gen_fonts.py` sinh `src/GUI/font_data.c` từ DejaVu Sans (giấy phép Bitstream Vera).
- `tools/gui_preview.c` vẽ thử các màn trên máy tính:
  `cc -Isrc/GUI tools/gui_preview.c src/GUI/GUI.c src/GUI/font_data.c -o build/gui_preview`.

## Giao thức chỉ đường
| Lệnh | Nội dung |
|---|---|
| `40 ff ii dd dd rr rr hh mm text…` | cờ (`01` ép refresh toàn màn, `02` đã đến, `04` sắp rẽ – hoãn refresh toàn màn định kỳ, `08` đường thẳng dài – nên dọn bóng mờ ngay), icon, khoảng cách (m, LE), còn lại (×10 m, LE), giờ:phút đến, chữ UTF-8 ≤160 byte |
| `42 jx jy mpp w n x y …` | sơ đồ ngã rẽ cho các gói `40` có cờ `10`: tâm ngã rẽ, mét/điểm ảnh ×10, rồi các đường (độ dày, số điểm, toạ độ trong khung 138×78 dưới dòng chữ); đường cuối là tuyến đi, có mũi tên |
| `41 00` | dừng chỉ đường, quay lại đồng hồ/lịch |
| `41 01 xx` | refresh nhanh bật (`01`) / tắt (`00`) |
| `41 02 nn` | refresh toàn màn sau `nn` lần refresh nhanh |
| `41 04 vv` | cách refresh nhanh: 0 LUT từ firmware, 1 trình tự Waveshare, 2 LUT OTP mode 2 của màn |
| `41 03 oo` | căn dọc: cột RAM bắt đầu ở byte `oo` (mặc định 1 = source 8, khớp kính AESL0213C) |

Icon: 0 thẳng, 1 trái, 2 phải, 3 chếch trái, 4 chếch phải, 5 gắt trái, 6 gắt phải, 7 nhánh trái,
8 nhánh phải, 9 quay đầu trái, 10 quay đầu phải, 11 vòng xuyến, 12 nhập làn, 13 đích.
