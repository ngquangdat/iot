# AESL0213C Controller

Web điều khiển nhãn điện tử **AESL0213C** (Nordic nRF52811, e-paper 2.13" 250×128, 3 màu đen/trắng/đỏ) qua **Web Bluetooth**. Chỉ gồm một file `index.html` tĩnh, không cần server.

## Tính năng
- Quét và kết nối BLE, đọc phiên bản firmware, model ID, panel, chế độ hiện tại
- Chuyển chế độ **Lịch / Đồng hồ / Ảnh** và đồng bộ giờ (có múi giờ), đổi ngày đầu tuần
- Đọc/đặt **lịch ngủ màn hình**
- Gửi ảnh 250×128: kéo thả/dán ảnh, vừa khung/phủ kín/kéo giãn, xoay, zoom, độ sáng, tương phản,
  dithering (Floyd–Steinberg, Atkinson, Bayer, ngưỡng), bảng màu BWR hoặc BW
- **Mẫu nhãn giá** (tên, giá, đơn vị, giá cũ gạch ngang, dòng phụ) và lớp chữ kéo thả được
- Kiểm tra/gửi mã kích hoạt, gửi lệnh hex thô, nhật ký
- **Chỉ đường** (tab "Chỉ đường"): lấy tuyến từ Google Routes API (API key của bạn, lưu trong trình duyệt)
  hoặc OpenStreetMap (miễn phí), bám GPS, tự tính lại tuyến khi đi lệch, hiển thị mũi tên + khoảng cách +
  tên đường lên nhãn. Có chế độ mô phỏng để thử tại chỗ.

## Chỉ đường trên iPhone
Safari không hỗ trợ Web Bluetooth, hãy mở trang bằng ứng dụng **Bluefy** và cho phép quyền vị trí. Trang phải
luôn mở ở màn hình chính (iOS tạm dừng web khi chạy nền). Với firmware hiện tại mỗi lần cập nhật nhãn là một lần
refresh 3 màu (~15–20 s), nên web chỉ gửi khi sang hướng rẽ mới hoặc khi vượt các mốc khoảng cách.

Google API key: bật **Routes API**, giới hạn key theo website `https://ngquangdat.github.io/*` và theo API.

## Chạy
Web Bluetooth cần HTTPS (hoặc `localhost`) và Chrome/Edge (desktop, Android). Trên iOS dùng Bluefy.

- **GitHub Pages**: Settings → Pages → Deploy from branch → chọn nhánh và thư mục `/ (root)`.
- **Cục bộ**: `python3 -m http.server 8000` rồi mở `http://localhost:8000`.

## Giao thức
Service `62750001-d828-918d-fb46-b6c11c675aec`, characteristic lệnh/notify `62750002-…`, phiên bản firmware `62750003-…`.

| Lệnh | Ý nghĩa |
|---|---|
| `01` | Khởi tạo màn hình |
| `05` | Refresh |
| `20 tttttttt zz mm` | Đặt giờ (unix big-endian), múi giờ (giờ), chế độ 0 ảnh · 1 lịch · 2 đồng hồ |
| `21 ww` | Ngày đầu tuần: 0 Chủ Nhật · 1 Thứ 2 |
| `30 pp data…` | Ghi ảnh. `pp` = (`00` gói đầu / `f0` gói tiếp) OR (`0f` lớp đen / `00` lớp đỏ) |
| `fb ff` / `fb e sh sm eh em` | Đọc / đặt lịch ngủ |
| `a0` / `a0 cc cc` | Truy vấn / gửi mã kích hoạt |

Dữ liệu ảnh mỗi lớp 4000 byte, sắp xếp theo cột từ x = 249 về 0, mỗi cột 128 điểm (16 byte, MSB trước).
Lớp đen: bit 1 = không đen. Lớp đỏ: bit 0 = đỏ. Kích thước gói mặc định 128 byte, tự chỉnh khi firmware báo `mtu=`.
