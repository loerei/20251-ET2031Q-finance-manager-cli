# 20251-ET2031Q-Finance Manager

Trình quản lý tài chính cá nhân CLI bằng C++17 để theo dõi giao dịch, lịch lặp, phân bổ theo danh mục và lãi suất. Hỗ trợ giao diện đa ngôn ngữ và đường dẫn lưu trữ linh hoạt.

## Tính năng
- Ghi nhận giao dịch thu/chi thủ công kèm ghi chú
- Lịch lặp (mỗi X ngày hoặc hàng tháng vào một ngày cố định)
- Phân bổ danh mục với số dư theo từng danh mục
- Quy tắc lãi theo danh mục (theo tháng hoặc theo năm)
- Cài đặt tự lưu, tự xử lý khi khởi động và ngôn ngữ
- Định dạng lưu nguyên tử với cơ chế escape và bảo vệ khôi phục
- Giải quyết đường dẫn lưu trữ linh hoạt (chạy được từ mọi thư mục làm việc)
- Bộ nạp i18n tìm locale trong các thư mục con

## Cấu trúc dự án
- `src/` - mã nguồn theo phiên bản (mới nhất: `finance_v3_0.cpp`) và mã nguồn thử nghiệm
- `bin/` - các file thực thi dựng sẵn (mới nhất: `finance_v3_0.exe`)
- `config/` - `i18n.h` và các file locale trong `config/locales/`
- `data/save/` - dữ liệu lưu bền vững (`finance_save.txt`)
- `docs/` - tài liệu hướng dẫn khởi chạy
- `Reference/` - tài liệu thử nghiệm và script hỗ trợ (bao gồm trình chạy lưu nguyên tử)
- `output/` - đầu ra debug tùy chọn
- `tests/` - dành cho các bài test bổ sung (hiện trống)

## Build
Từ thư mục gốc của dự án:
```bash
g++ -std=c++17 src/finance_v3_0.cpp -o bin/finance_v3_0.exe
```

## Run
```bash
bin/finance_v3_0.exe
```

Ứng dụng tự xác định thư mục gốc dự án khi chạy, nên có thể khởi chạy từ bất kỳ thư mục làm việc nào.

## Helper flags
- `--dump-loc <CODE>`: in một số khóa từ locale
- `--list-locales`: liệt kê các file locale đã tải và mã ngôn ngữ khả dụng
- `--dump-settings`: in cài đặt hiện tại từ tệp lưu
- `--test-balance-load`: kiểm tra hồi quy cho việc tính lại số dư

## Localization
File locale nằm trong `config/locales/` và có thể tách thành file gốc và file `_extra`. Phiên bản 3.0 quét thêm các thư mục con dưới `config/locales/` để nạp gói locale. Dùng `--list-locales` để kiểm tra các locale đã được nạp.

## Lưu
Tệp lưu mặc định:
```
data/save/finance_save.txt
```
Tệp sẽ được tạo tự động khi chạy lần đầu.

## Lịch sử phiên bản
- Xem `CHANGELOG.md` cho các phiên bản từ 1.0 đến 3.0.
- Mã nguồn và file thực thi mới nhất: `src/finance_v3_0.cpp` và `bin/finance_v3_0.exe`.

## Flowchart
- Bảng Miro: [Miro Flowchart](https://miro.com/app/board/uXjVGd4fsqQ=/?share_link_id=714558277659)
