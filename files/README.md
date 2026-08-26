# ESP32-S3 CNC Wi-Fi Bridge

Firmware này biến ESP32-S3 thành một bộ chuyển G-code có giao diện web: nạp file lên thẻ SD, chọn file, chạy/tạm dừng/tiếp tục/hủy, và gửi từng dòng tới bộ điều khiển CNC **tương thích GRBL qua UART**. Giao diện có HTTP Basic Authentication và chạy hoàn toàn trong mạng nội bộ, không phụ thuộc cloud.

> Cảnh báo an toàn: đây **không phải** là bộ điều khiển an toàn chức năng (safety-rated controller). Nút dừng khẩn, công tắc cửa, contactor/enable và giới hạn hành trình phải là mạch dây cứng độc lập với ESP32, đủ để dừng chuyển động khi ESP32, Wi-Fi hoặc phần mềm lỗi. Không đưa web server này thẳng ra Internet hoặc mở port-forward.

## 1. Kiến trúc

```text
Điện thoại / PC trên LAN hoặc VPN
              │ HTTPS/VPN ở tầng mạng (khuyến nghị)
              ▼
       Web UI có mật khẩu
              │ Wi-Fi
              ▼
 ESP32-S3 ── SPI ── SD card (G-code)
     │
     ├── PC817 (chỉ giám sát input: cửa, trạng thái…)
     │
     └── UART cách ly hai chiều ── GRBL RX/TX ── CNC controller

E-stop NC ── safety relay/contactor ── CNC enable / nguồn driver
             (đường phần cứng riêng, không đi qua ESP32)
```

Firmware gửi một lệnh G-code, chờ GRBL trả `ok`, rồi mới gửi lệnh kế tiếp. Cách này chậm hơn chiến lược lấp đầy serial buffer nhưng tránh tràn bộ đệm và dễ chẩn đoán lỗi.

## 2. Phần cứng cần có

Bạn đã có ESP32-S3, module SD và PC817. Để kết nối CNC an toàn, vẫn cần các phần sau:

| Hạng mục | Mục đích | Ghi chú |
|---|---|---|
| Nguồn 5 V ổn áp cho ESP32-S3 | Cấp nguồn logic | Chọn dòng đủ cho board và SD; không lấy từ nguồn driver nhiễu nếu chưa lọc tốt. |
| Module SD **tương thích logic 3.3 V** | Lưu G-code | Không để chân MISO/logic 5 V đi vào ESP32-S3. |
| Cách ly UART 2 chiều | Bảo vệ đường ESP32 ↔ controller | Khuyên dùng ADuM1201/ADuM120N hoặc mạch 6N137/6N138 hai chiều có cách ly nguồn đúng cách. |
| Nguồn DC-DC cách ly cho phía CNC (nếu module yêu cầu) | Giữ hai miền mass tách biệt | Chọn theo datasheet của mạch cách ly UART. |
| E-stop NC, safety relay/contactor, công tắc cửa | Dừng an toàn bằng phần cứng | Bắt buộc cho máy có chuyển động thực. Không thay bằng PC817 hoặc nút trên web. |

### Vì sao không dùng PC817 để truyền UART CNC?

PC817 là opto transistor chậm và CTR thay đổi theo nhiệt độ/linh kiện. Nó phù hợp hơn cho ngõ vào trạng thái chậm (nút nhấn, cửa, relay, limit status) và **không đáng tin cậy ở UART 115200 baud** của GRBL. Firmware mặc định 115200 baud, vì vậy cần mạch cách ly UART tốc độ cao riêng. Đừng nối GPIO trực tiếp vào cổng 5 V hoặc cổng công nghiệp của CNC.

Chỉ khi controller được nhà sản xuất xác nhận dùng UART TTL 3.3 V và dùng chung mass logic, có thể nối trực tiếp để thử trên bàn. Đó không phải cấu hình chống nhiễu/cách ly phù hợp cho máy đang vận hành.

## 3. Đấu nối ESP32-S3

### SD card qua SPI

| SD | ESP32-S3 | Cấu hình |
|---|---:|---|
| CS | GPIO10 | `SD_CS_PIN` |
| SCK | GPIO12 | `SD_SCK_PIN` |
| MOSI / DI | GPIO11 | `SD_MOSI_PIN` |
| MISO / DO | GPIO13 | `SD_MISO_PIN` |
| VCC | 3.3 V | Chỉ dùng module bảo đảm logic 3.3 V |
| GND | GND logic | Chung mass với ESP32 |

Format thẻ thành FAT32. Trước khi nạp file thật, kiểm tra Serial Monitor phải báo `[SDLogger] The SD san sang.`

### UART tới GRBL

| ESP32-S3 | Qua mạch cách ly UART | GRBL controller |
|---|---|---|
| GPIO17 (TX) | TX logic → RX phía CNC | RX |
| GPIO18 (RX) | RX logic ← TX phía CNC | TX |
| 3.3 V / GND logic | phía logic của module | theo datasheet module |

Chéo TX/RX. Baud mặc định là 115200, 8N1. Nếu controller của bạn không phải GRBL-compatible UART (ví dụ USB-only, Mach3, Syntec hoặc giao thức độc quyền), firmware này không được phép nối trực tiếp; cần gateway/giao thức phù hợp trước.

### PC817 4 kênh: chỉ giám sát

| PC817 output (phía ESP32) | ESP32-S3 |
|---|---:|
| OUT1 | GPIO4 |
| OUT2 | GPIO5 |
| OUT3 | GPIO6 |
| OUT4 | GPIO7 |
| VCC logic | 3.3 V nếu module hỗ trợ |
| GND logic | GND ESP32 |

Nhiều board PC817 có logic active-low; cấu hình mặc định `ISO_INPUT_ACTIVE_LOW true` đã phản ánh kiểu đó. Xác nhận sơ đồ in trên đúng module của bạn trước khi cấp nguồn. Phía field của opto phải có nguồn/mass tách biệt thật sự nếu cần isolation; nhiều module giá rẻ không tách hoàn toàn như tên gọi của chúng.

Có thể gán một kênh làm giám sát E-stop/cửa trong `config.h` (`ESTOP_INPUT_CHANNEL`, `DOOR_INPUT_CHANNEL`). Khi active, firmware lần lượt gửi soft reset hoặc feed hold. Đây chỉ là lớp bổ sung: E-stop dây cứng vẫn phải cắt enable/nguồn driver.

## 4. Cấu hình trước khi nạp

Mở [`config.h`](config.h) và thay tất cả giá trị `CHANGE_ME`:

```cpp
#define WIFI_SSID "ten_wifi_xuong"
#define WIFI_PASSWORD "mat_khau_wifi"
#define FALLBACK_AP_PASSWORD "mat_khau_AP_it_nhat_8_ky_tu"
#define WEB_USERNAME "admin"
#define WEB_PASSWORD "mat_khau_web_rieng"
```

`WEB_PASSWORD` phải khác mẫu mặc định và dài tối thiểu 12 ký tự; nếu không, dashboard/API sẽ trả lỗi 503 và không cho điều khiển máy.

Chỉnh `CNC_BAUD` chỉ khi GRBL của bạn thật sự dùng tốc độ khác. Không đổi GPIO khi chưa đối chiếu pinout board ESP32-S3 của bạn (một số board dùng GPIO cho PSRAM/flash).

## 5. Nạp firmware

### Arduino IDE

1. Cài ESP32 by Espressif Systems trong Board Manager.
2. Mở `ESP32S3_Isolated_Logger.ino` trong thư mục này.
3. Chọn board **ESP32S3 Dev Module** (hoặc model S3 chính xác), đúng cổng COM, rồi Upload.
4. Mở Serial Monitor 115200 baud để lấy IP và xác minh SD/UART.

Các thư viện `WiFi`, `WebServer`, `ESPmDNS`, `SD`, `SPI` là một phần của ESP32 Arduino Core; không phải cài thêm thư viện ngoài.

### PlatformIO (tùy chọn)

Đã có [`platformio.ini`](platformio.ini) cho `esp32-s3-devkitc-1`:

```powershell
C:\Users\Admin\.platformio\penv\Scripts\platformio.exe run
C:\Users\Admin\.platformio\penv\Scripts\platformio.exe run --target upload
```

Nếu board S3 của bạn có flash/PSRAM khác DevKitC-1, chọn đúng `board` trong `platformio.ini` trước khi nạp.

## 6. Dùng giao diện web

- Nếu ESP32 vào được Wi-Fi: Serial Monitor in địa chỉ `http://<IP>/`; có thể thử `http://cnc-bridge.local/` trên mạng hỗ trợ mDNS.
- Nếu không vào được Wi-Fi hoặc chưa cấu hình: kết nối AP `CNC-Bridge-Setup`, dùng mật khẩu AP đã đặt, mở `http://192.168.4.1/`.
- Trình duyệt sẽ hỏi username/password web. Sau khi đăng nhập, nạp tệp `.nc`, `.gcode`, `.tap`, hoặc `.txt`, rồi chọn **Chạy**.

Dashboard hiển thị trạng thái job, dòng nguồn/đã gửi, trạng thái GRBL, phản hồi GRBL và bốn input PC817. Khi đã gửi hết dòng, trạng thái là `finishing` cho đến khi GRBL báo `Idle`; trong thời gian đó job vẫn có thể bị Hủy. Nó không tự chạy lại job sau reset/mất điện, không tự resume sau cửa mở, và không tự unlock alarm GRBL.

Các API dùng nội bộ bởi web UI:

| Endpoint | Chức năng |
|---|---|
| `GET /api/status` | trạng thái SD, mạng, input, job |
| `GET /api/files` | danh sách G-code |
| `POST /api/upload` | multipart upload trường `uploadFile` |
| `POST /api/job/start` | `file=<name>` |
| `POST /api/job/pause`, `/resume`, `/abort` | điều khiển job |
| `POST /api/file/delete` | `file=<name>` |

## 7. Quy trình kiểm tra an toàn

1. **Chưa nối motor hoặc tháo dao ra trước.** Kiểm tra mạch E-stop dây cứng cắt được enable/nguồn driver dù ESP32 mất nguồn.
2. Đo xác nhận điện áp và chiều TX/RX trước khi cắm controller. Không có chân ESP32 nào được nhận quá 3.3 V.
3. Bật ESP32, kiểm tra SD và mạng qua Serial Monitor. Mở dashboard; thử từng PC817 input để xem ô `IN 1…4` đổi trạng thái.
4. Kết nối controller ở trạng thái an toàn. Dùng một G-code mô phỏng rất ngắn, tọa độ an toàn, feed chậm và quan sát phản hồi `ok`.
5. Kiểm tra Tạm dừng, Tiếp tục và Hủy. Sau Hủy hoặc lỗi `alarm/error`, phải re-home/kiểm tra trạng thái máy theo quy trình của bạn trước job mới.
6. Chỉ sau khi các bước trên đạt, mới chạy job thật có giám sát tại máy.

## 8. Vận hành từ xa

Giao diện được thiết kế cho LAN tin cậy. Để truy cập từ nơi khác, hãy vào mạng nhà xưởng qua VPN do router/firewall quản lý; vẫn phải có người được ủy quyền ở gần máy cho mọi job có chuyển động. Không dùng port forwarding trực tiếp đến ESP32 vì firmware không cung cấp TLS, phân quyền nhiều người dùng hay audit trail công nghiệp.

## 9. Cấu trúc mã nguồn

```text
ESP32S3_Isolated_Logger.ino  # khởi động Wi-Fi, mDNS, web và vòng lặp an toàn
config.h                     # chân, Wi-Fi, mật khẩu, UART, input safety
GCodeStreamer.*              # bộ gửi G-code GRBL tuần tự, xử lý ok/error/timeout
WebInterface.*               # dashboard + API xác thực + upload SD an toàn
IsolatedInputs.*             # đọc PC817 và debounce
SDLogger.*                   # log biến đổi input vào /isolated_log.csv
platformio.ini               # cấu hình build PlatformIO tùy chọn
```

### Giới hạn có chủ đích

- Mỗi dòng G-code tối đa 255 ký tự; comment `;...` và `( ... )` được bỏ khi gửi.
- Chỉ một job một lúc; không cho ghi đè/xóa file đang chạy.
- Khi mất phản hồi controller quá 30 giây, job chuyển `failed`; thao tác dừng/cố chạy lại cần do người vận hành quyết định.
- Firmware giả định GRBL trả `ok`, `error`, `alarm` theo dòng. Nó không thay thế kiểm tra trạng thái/homing, CAM verification, soft limit hay interlock của CNC.
