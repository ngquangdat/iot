# Firmware AESL0213C (nRF52811) – chỉ đường

Firmware mở rộng từ [tsl0922/EPD-nRF5](https://github.com/tsl0922/EPD-nRF5) (GPL-3.0, commit `7e31961`)
cho nhãn AESL0213C: nRF52811 + SoftDevice S112 7.3.0, màn 2.13" SSD1680 (122×250, đen/trắng/đỏ).

> ⚠️ **Chưa thử trên phần cứng thật.** Đã build, kiểm tra kích thước/bố cục bộ nhớ, chữ ký gói OTA và
> xem trước giao diện trên máy tính (cùng mã C). Nạp là thay thế firmware hiện tại, không quay lại được.

## Có gì mới
- **Lệnh chỉ đường `0x40`**: web gửi vài chục byte (icon, khoảng cách, tên đường, còn lại, giờ đến),
  nhãn tự vẽ bằng font tiếng Việt. Các lệnh đến dồn dập được gộp, chỉ vẽ bản mới nhất.
- **Refresh nhanh đen trắng** (thử nghiệm, LUT nạp từ host) cho chỉ đường và đồng hồ, cứ 20 lần thì
  refresh toàn màn một lần để xoá bóng mờ. Tắt/bật và đổi chu kỳ trong tab *Nâng cao* của web (`0x41`).
- Màn **Đồng hồ** và **Lịch** (ngày, thứ tiếng Việt, nhiệt độ, điện áp pin) thay cho lịch âm tiếng Trung.
- Giữ tương thích với web hiện tại: model 2, gói cấu hình báo panel `0x21`, lệnh ghi ảnh `0x30`
  cùng cách đặt cờ (`0x0f` lớp đen, `0xf0` gói tiếp theo), đồng bộ giờ `0x20`, lịch/đồng hồ.
- Vẫn có **nạp qua Bluetooth không cần nút** (Secure DFU) để cập nhật lần sau.

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
| `40 ff ii dd dd rr rr hh mm text…` | cờ (`01` ép refresh toàn màn, `02` đã đến), icon, khoảng cách (m, LE), còn lại (×10 m, LE), giờ:phút đến, chữ UTF-8 ≤160 byte |
| `41 00` | dừng chỉ đường, quay lại đồng hồ/lịch |
| `41 01 xx` | refresh nhanh bật (`01`) / tắt (`00`) |
| `41 02 nn` | refresh toàn màn sau `nn` lần refresh nhanh |

Icon: 0 thẳng, 1 trái, 2 phải, 3 chếch trái, 4 chếch phải, 5 gắt trái, 6 gắt phải, 7 nhánh trái,
8 nhánh phải, 9 quay đầu trái, 10 quay đầu phải, 11 vòng xuyến, 12 nhập làn, 13 đích.
