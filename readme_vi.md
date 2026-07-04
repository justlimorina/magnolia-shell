# Magnolia Shell (`mash`)

Một shell Unix gọn nhẹ, giàu tính năng được viết bằng C++17. Dự án được thiết kế hướng tới sự sạch sẽ, phản hồi nhanh và độc lập hoàn toàn khỏi các thư viện tương tác cồng kềnh (như readline), bằng cách sử dụng trực tiếp các POSIX system APIs và cấu hình chế độ terminal raw mode.

## Tính năng

### 1. Biên tập Dòng lệnh Tương tác & Tự động hoàn thành (Autocomplete)
- **Tab Completion**: Tự động điền tiếp tên lệnh (built-ins & tệp thực thi trong đường dẫn `$PATH`) và đường dẫn tệp/thư mục. Nhận diện thư mục và tự động thêm dấu gạch chéo `/` ở cuối. Hỗ trợ hiển thị danh sách gợi ý khi nhấn Tab 2 lần.
- **Lịch sử Lệnh (History)**: Duyệt lại các lệnh đã thực thi bằng phím mũi tên **Up** và **Down**. Bản nháp dở dang của lệnh hiện tại được tự động bảo lưu khi duyệt lịch sử lên xuống.
- **Di chuyển Con trỏ**: Di chuyển vị trí soạn thảo bằng mũi tên **Left** và **Right**, hoặc nhảy nhanh về đầu/cuối dòng bằng phím **Home** và **End**.
- **Biên soạn nhanh**: Chỉnh sửa chuỗi nhập liệu tại vị trí con trỏ bằng phím **Backspace** và **Delete**.
- **Hỗ trợ Ctrl+C / Ctrl+D**: Nhấn `Ctrl+C` hủy dòng lệnh hiện tại mà không làm tắt shell. Nhấn `Ctrl+D` khi dòng nhắc trống để thoát shell sạch sẽ.

### 2. Đường ống (Pipelines) & Chuyển hướng xuất nhập (Redirections)
- **Đường ống (`|`)**: Truyền luồng đầu ra của lệnh này làm đầu vào cho lệnh khác (ví dụ: `uptime | grep uptime`).
- **Chuyển hướng tệp (Redirection)**: Nhận đầu vào từ tệp bằng `<` và ghi xuất đầu ra bằng `>` (ghi đè) hoặc `>>` (ghi nối tiếp) (ví dụ: `echo "dữ liệu" > log.txt`).
- **Mở rộng Ký tự đại diện (`*`)**: Tự động chuyển đổi các mẫu globbing như `ls header/*.h` hoặc `rm *.o` thành danh sách tập tin khớp thực tế.

### 3. Kiểm soát Tín hiệu & Chế độ Terminal ổn định
- Tự động hoàn trả terminal về chế độ cooked mode tiêu chuẩn trước khi chạy tiến trình con, đảm bảo các phần mềm toàn màn hình tương tác (như `nano`, `vim`, hoặc lệnh yêu cầu nhập mật khẩu như `sudo`) hoạt động ổn định.
- Shell cha chủ động bỏ qua tín hiệu `SIGINT` (Ctrl+C) khi đang chờ tiến trình con thực thi, giúp bạn tắt tác vụ con mà không làm tắt cả shell.
- Khôi phục chế độ terminal mặc định khi thoát hoặc khi chương trình gặp lỗi chấm dứt đột ngột.

### 4. Phân tích Dòng lệnh Thông minh
- **Hỗ trợ Dấu ngoặc kép**: Nhận diện tham số bọc trong nháy đơn `'...'` hoặc nháy kép `"..."` (ví dụ: `mkdir "my directory"`).
- **Ký tự Escape**: Hỗ trợ dấu gạch chéo ngược `\` để xử lý các khoảng trắng hoặc ký tự đặc biệt trong đường dẫn.
- **Mở rộng Tilde**: Tự động chuyển đổi ký tự `~` thành đường dẫn thư mục Home của người dùng hiện tại.
- **Biến môi trường**: Tự động mở rộng các chuỗi biến `$VAR` (ví dụ: `$USER`, `$HOME`) trong câu lệnh hoặc chuỗi nháy kép.

### 5. Bộ sưu tập các Lệnh Built-in Cốt lõi phong phú
Bên cạnh việc thực thi tệp chương trình bên ngoài qua `execvp`, `mash` tích hợp trực tiếp các lệnh hệ thống để tăng tốc độ chạy và hoạt động độc lập:
- **Thông tin & Điều hướng**: `cd`, `pwd`, `ls` (dạng bảng căn chỉnh màu sắc), `which`, `uptime`, `free`.
- **Tập tin & Thư mục**: `mkdir`, `rm`, `touch`, `cp`, `mv`, `cat`, `head`, `tail`, `find` (tìm đệ quy theo tên).
- **Quyền hạn & Sở hữu**: `chmod`, `chown`, `chgrp`.
- **Tiện ích Hệ thống**: `echo`, `date`, `uname`, `whoami`, `version`, `help`, `history`, `clear`.
- **Quản lý Môi trường & Alias**: `env`, `export`, `alias`, `unalias`, `source` (chạy tệp script shell).

---

## Yêu cầu Hệ thống

- Bộ biên dịch hỗ trợ chuẩn C++17 trở lên (ví dụ: `g++` 8+ hoặc `clang++`).
- Môi trường POSIX chuẩn (Linux hoặc macOS).

---

## Bắt đầu Sử dụng

### Tải Binary Đã Biên Dịch Sẵn
Bạn có thể tải trực tiếp file thực thi đã biên dịch sẵn từ trang **Releases** của repository này:
- **Linux (amd64)**: `mash-linux-amd64`
- **macOS (Universal - Intel & Apple Silicon)**: `mash-darwin-universal`

Sau khi tải về, cấp quyền thực thi và chạy:
```bash
chmod +x mash-linux-amd64 # hoặc mash-darwin-universal
./mash-linux-amd64
```

### Tự Biên Dịch Từ Nguồn
Biên dịch dự án thông qua tập lệnh build có sẵn:
```bash
./build.sh
```

### Chạy Shell
Khởi chạy Magnolia Shell:
```bash
./mash
```

---

## Giấy phép (License)

Dự án này được phân phối dưới giấy phép MIT License. Xem chi tiết tại tệp [LICENSE](LICENSE).
