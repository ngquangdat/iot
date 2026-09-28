# Nhãn chỉ đường AESL0213C

Web điều khiển nhãn e-paper **AESL0213C** (Nordic nRF52811, màn 2.13" 250×128 đen/trắng/đỏ) qua Bluetooth.
Một file `index.html`, không cần server. Mở tại **https://ngquangdat.github.io/iot/**.

## Chức năng
- **Chỉ đường** – nhập điểm đến, chọn phương tiện, bấm *Bắt đầu*. Tuyến lấy từ Google Maps (Routes API, key của bạn)
  hoặc OpenStreetMap (miễn phí). Web bám GPS, tự tính lại khi đi lệch. Trong vùng sắp rẽ (mặc định 500 m, chỉnh trong
  Cài đặt) nhãn đếm ngược liên tục ~1,5 giây/lần — web ước lượng vị trí giữa các lần GPS theo tốc độ đang đi.
- **Màn hình** – chuyển nhãn sang *Đồng hồ* hoặc *Lịch*, hoặc gửi một ảnh (tự cắt vừa khung, chuyển sang 3 màu).
- **Cài đặt** – nguồn bản đồ và API key, chế độ thử (giả lập di chuyển), tuỳ chỉnh cập nhật nhanh của nhãn,
  liên kết tải firmware, nhật ký.
- Tự đồng bộ giờ khi kết nối; giao diện sáng/tối theo hệ thống.

## Dùng trên iPhone
Safari không có Web Bluetooth — mở trang bằng ứng dụng **Bluefy** và cho phép vị trí. Khi chỉ đường, giữ trang mở
trên màn hình (iOS tạm dừng web chạy nền). Android/máy tính: Chrome hoặc Edge.

Google API key: bật **Routes API**, giới hạn key theo website `https://ngquangdat.github.io/*`. Key chỉ lưu trong trình duyệt.

## Firmware
Thư mục [`firmware/`](firmware/README.md) chứa firmware cho nhãn (nạp OTA qua Bluetooth bằng *nRF Device Firmware Update*):
nhãn tự vẽ màn chỉ đường với cập nhật nhanh ~1,5 giây. Với firmware gốc, web vẫn chạy nhưng mỗi lần cập nhật là
một lần refresh 3 màu (~15 giây).
