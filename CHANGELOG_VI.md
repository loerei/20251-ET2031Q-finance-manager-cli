# Finance Manager - Nhật ký thay đổi đầy đủ

**Cập nhật lần cuối:** Tháng 2, 2026

---

## Phiên bản 1.0 (Nền tảng)
**Tệp:** `finance_v1_0.cpp` (449 dòng)  
**Trạng thái:** Phát hành ban đầu

### Tính năng cốt lõi đã triển khai:
- **Quản lý giao dịch** (Dòng 18-23)
  - Theo dõi giao dịch thủ công với ngày, số tiền, danh mục, ghi chú
  - Số dương = thu nhập, số âm = chi phí
  
- **Hệ thống lịch** (Dòng 25-32)
  - Hai loại lịch: EveryXDays, MonthlyDay
  - Giao dịch lặp kèm tùy chọn tự phân bổ
  
- **Hệ thống danh mục**
  - Theo dõi danh mục cơ bản
  - Tự phân bổ theo phần trăm giữa các danh mục
  
- **Tính lãi** (Dòng 35+)
  - Tính lãi tiết kiệm cơ bản
  
- **Lưu trữ** (Dòng 40+)
  - Lưu/Tải từ "finance_save.txt"
  
- **Xử lý ngày**
  - Phân tích và định dạng ngày (YYYY-MM-DD)
  - Tích hợp chrono cơ bản

### Hàm chính:
- `parseDate()` - Phân tích định dạng YYYY-MM-DD
- `toDateString()` - Định dạng ngày thành chuỗi
- Quản lý giao dịch & lịch cơ bản

---

## Phiên bản 1.1 (An toàn luồng & Escape)
**Tệp:** `finance_v1_1.cpp`
**Trạng thái:** Cải tiến nền tảng

### Thay đổi so với 1.0:
**Tính năng mới/cải tiến:**

1. **Localtime an toàn** (Dòng 36-47)
   - Cài đặt localtime thread-safe theo nền tảng
   - Hỗ trợ Windows/MinGW, POSIX, macOS, Linux
   - Thay thế gọi `localtime()` không an toàn

2. **Cơ chế escape** (Dòng ~200-250)
   - Mới: `escapeForSave()` - Escape ký tự đặc biệt (\\, |, \n)
   - Mới: `unescapeLoaded()` - Khôi phục ký tự đã escape
   - Mới: `splitEscaped()` - Phân tích dữ liệu phân tách bằng dấu `|` có escape
   - Tránh hỏng định dạng lưu phân tách bằng `|`

3. **Lưu/Tải vững chắc hơn**
   - Triển khai định dạng phân tách bằng `|` với escape đầy đủ
   - Xử lý ghi chú và danh mục có ký tự đặc biệt

### Hàm thêm:
- `safeLocaltime()` - Xử lý thời gian thread-safe
- `escapeForSave()` - Escape ký tự
- `unescapeLoaded()` - Unescape ký tự
- `splitEscaped()` - Phân tích trường an toàn

---

## Phiên bản 1.2 (Cải thiện UI)
**Tệp:** `finance_v1_2.cpp`
**Trạng thái:** Nâng cấp UX

### Thay đổi so với 1.1:
- Thêm luồng `askReturnToMenu()` để người dùng chọn tiếp tục hoặc thoát sau thao tác.
- Chuẩn hóa kiểm tra đầu vào cho ngày, số tiền và tham số lịch.
- Chỉnh sửa câu chữ menu và prompt cho rõ ràng hơn.

---

## Phiên bản 1.3 (Sửa ngày phân bổ)
**Tệp:** `finance_v1_3.cpp`
**Trạng thái:** Sửa lỗi độ chính xác dữ liệu

### Thay đổi so với 1.2:
- Tự phân bổ ghi đúng ngày giao dịch cho cả giao dịch thủ công và giao dịch theo lịch.
- Tự phân bổ theo lịch dùng ngày của lịch thay vì "hôm nay".
- Dọn dẹp nhỏ về prompt và luồng.

---

## Phiên bản 1.4 (Chuẩn hóa danh mục)
**Tệp:** `finance_v1_4.cpp`
**Trạng thái:** Nâng cấp mô hình danh mục

### Thay đổi so với 1.3:
- Chuẩn hóa khóa danh mục với tên hiển thị để tránh trùng do khác hoa/thường hoặc khoảng trắng.
- Lịch mang danh mục tùy chọn (hỗ trợ chi phí định kỳ có danh mục cụ thể).
- Chọn danh mục theo tên hoặc số; tự phân bổ chỉ cho số dương.
- Số dư danh mục được tính lại từ giao dịch khi tải để đảm bảo nhất quán.

---

## Phiên bản 2.0 (Thiết lập hướng dẫn & kiểm soát phân bổ)
**Tệp:** `finance_v2_0.cpp`
**Trạng thái:** Mở rộng tính năng

### Thay đổi so với 1.4:
- Thêm thiết lập danh mục có hướng dẫn và thiết lập phân bổ (Other là phần còn lại).
- Cải thiện xử lý danh mục với sắp xếp theo tên hiển thị và chuẩn hóa.
- Chỉnh sửa phân bổ hoạt động trên danh mục hiện có để an toàn hơn.

---

## Phiên bản 2.1 (Lãi theo danh mục)
**Tệp:** `finance_v2_1.cpp`
**Trạng thái:** Mở rộng logic tài chính

### Thay đổi so với 2.0:
- Thêm mục lãi theo danh mục với lãi suất tháng hoặc năm và ngày bắt đầu.
- Thêm xử lý lãi theo tháng với logic gộp lãi.
- Menu lãi mới: thêm/cập nhật/xóa lãi suất và áp dụng lãi theo yêu cầu.

---

## Phiên bản 2.2 (Cài đặt + bản địa hóa EN/VI)
**Tệp:** `finance_v2_2.cpp`
**Trạng thái:** UX & Cài đặt

### Thay đổi so với 2.1:
- Thêm cài đặt tự lưu, tự xử lý khi khởi động, và ngôn ngữ.
- Thêm trợ giúp dịch EN/VI cho menu và prompt.
- Cài đặt được lưu trong tệp và áp dụng khi khởi động (auto-process).
- Tự lưu tích hợp vào luồng thoát/return-to-menu.

---

## Phiên bản 2.3 (Lưu nguyên tử + nhật ký thao tác)
**Tệp:** `finance_v2_3.cpp`
**Trạng thái:** Độ tin cậy & kiểm toán

### Thay đổi so với 2.2:
- Lưu nguyên tử qua file tạm + đổi tên để giảm nguy cơ hỏng dữ liệu.
- Thêm log thao tác append-only (`finance_full_log.txt`) cho audit/debug.
- Gộp giao dịch và snapshot để tránh trùng và hỗ trợ xử lý lịch/lãi an toàn hơn.

---

## Phiên bản 2.4 (Đường dẫn linh hoạt + i18n ngoài)
**Tệp:** `finance_v2_4.cpp`
**Trạng thái:** Bản địa hóa & Tính di động

### Thay đổi so với 2.3:
- Chuyển sang loader i18n ngoài (`config/i18n.h`) với danh sách ngôn ngữ động.
- Giải quyết working directory theo vị trí executable.
- Thêm cờ hỗ trợ: `--dump-loc`, `--list-locales`, `--dump-settings`.

---

## Phiên bản 2.5 (Refactor & phân tích đầu vào)
**Tệp:** `finance_v2_5.cpp`
**Trạng thái:** Refactor

### Thay đổi so với 2.4:
- Tổ chức lại code thành các phần trợ giúp tập trung (ngày, parsing, danh mục, phân bổ, lịch).
- Cải thiện helper phân tích và kiểm tra đầu vào số.
- Menu/cấu hình gọn hơn nhờ tái sử dụng tiện ích chung.

---

## Phiên bản 2.6 (UI console ANSI)
**Tệp:** `finance_v2_6.cpp`
**Trạng thái:** Hoàn thiện UI

### Thay đổi so với 2.5:
- Thêm helper ANSI terminal để xóa màn hình và điều khiển con trỏ.
- Bật ANSI escape sequences trên Windows console.
- Cập nhật hiển thị menu dùng helper terminal.

---

## Phiên bản 2.8 (Cải thiện locale)
**Tệp:** `finance_v2_8.cpp`
**Trạng thái:** Nâng cấp i18n

### Thay đổi so với 2.6:
- Thêm DE vào ngôn ngữ hỗ trợ trong dữ liệu i18n.
- Parsing lãi suất hỗ trợ dấu phẩy thập phân (vd: "0,5%").
- Mở rộng chuỗi bản địa hóa cho menu và cài đặt.

---

## Phiên bản 2.9 (Sửa tính lại số dư)
**Tệp:** `finance_v2_9.cpp`
**Trạng thái:** Độ chính xác dữ liệu

### Thay đổi so với 2.8:
- Tính lại số dư từ giao dịch khi tải nếu có giao dịch, tránh số dư lưu cũ.
- Thêm trợ giúp hồi quy `--test-balance-load`.
- Thêm các bảo vệ cho tính lại số dư theo danh mục.

---

## Phiên bản 3.0 (Tìm gói locale)
**Tệp:** `finance_v3_0.cpp`
**Trạng thái:** Bản phát hành mới nhất

### Thay đổi so với 2.9:
- Quét thư mục con dưới `config/locales` và `locales` để nạp gói locale bổ sung.
- Cải thiện xử lý UTF-8 trên Windows (code page console + thiết lập locale).
- Giữ lại cờ hỗ trợ và luồng cài đặt với độ phủ i18n mở rộng.
