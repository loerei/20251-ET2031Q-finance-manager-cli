# 1. Giới thiệu
- Tổng quan dự án: ứng dụng console C++17 để theo dõi tài chính cá nhân với giao dịch, lịch lặp, phân bổ theo danh mục và lãi suất theo danh mục. Triển khai chính nằm ở `src/finance_v3_0.cpp`.
- Mục tiêu: cho phép ghi nhận thu/chi, tự phân bổ thu nhập theo tỷ lệ danh mục, xử lý giao dịch theo lịch và áp dụng lãi; lưu dữ liệu vào tệp lưu cục bộ.
- Người dùng mục tiêu: cá nhân quản lý tài chính trong môi trường terminal/CLI.
- Tóm tắt tính năng mức cao : giao dịch thủ công, lịch lặp, quy tắc phân bổ, quy tắc lãi, cài đặt (tự lưu, tự xử lý khi khởi động, ngôn ngữ), lưu tệp, và giao diện đa ngôn ngữ qua file locale.

# 2. Yêu cầu và phạm vi
## Yêu cầu chức năng 
- Ghi nhận giao dịch thủ công với ngày, số tiền, danh mục và ghi chú. (`src/finance_v3_0.cpp::Account::addManualTransaction`)
- Định nghĩa lịch định kỳ (mỗi N ngày hoặc hàng tháng vào một ngày cụ thể) và xử lý đến một mốc ngày. (`src/finance_v3_0.cpp::Account::processSchedulesUpTo`)
- Phân bổ thu nhập theo tỷ lệ danh mục, với "Other" là phần còn lại. (`src/finance_v3_0.cpp::Account::allocateAmount`, `src/finance_v3_0.cpp::interactiveAllocSetup`)
- Cấu hình lãi theo danh mục (tháng hoặc năm quy đổi theo tháng) và áp dụng theo thời gian. (`src/finance_v3_0.cpp::Account::applyInterestUpTo`)
- Lưu và tải lại toàn bộ dữ liệu tài khoản từ tệp cục bộ. (`src/finance_v3_0.cpp::Account::saveToFile`, `src/finance_v3_0.cpp::Account::loadFromFile`)
- Cung cấp CLI theo menu có cài đặt và bản địa hóa. (`src/finance_v3_0.cpp::printMenu`, `src/finance_v3_0.cpp::settingsMenu`, `config/i18n.h::I18n`)

## Yêu cầu phi chức năng 
- Tính di động: dùng C++17 chuẩn và `std::filesystem`; sử dụng API console Windows dưới `#ifdef _WIN32`. (`src/finance_v3_0.cpp`)
- Tính toàn vẹn dữ liệu: kiểm tra đầu vào và phân tích phòng thủ cho tệp lưu; tính lại số dư từ giao dịch khi tải. (`src/finance_v3_0.cpp::tryParseDate`, `src/finance_v3_0.cpp::Account::loadFromFile`)
- Ràng buộc UX: chỉ CLI, theo menu, nhắc nhập văn bản; ESC hoặc "esc" có thể hủy một số luồng. (`src/finance_v3_0.cpp::getlineAllowEsc`)
- Bản địa hóa: khóa văn bản lấy từ file locale; có cơ chế fallback nếu thiếu. (`config/i18n.h::I18n::get`)

## Trong phạm vi và ngoài phạm vi
- Trong phạm vi: lưu tệp cục bộ, quy trình CLI một người dùng, logic lịch/lãi/phân bổ, chọn ngôn ngữ.
- Ngoài phạm vi (không thấy trong repo): tài khoản nhiều người dùng, đồng bộ đám mây, mã hóa, GUI, dashboard ngân sách, xuất bảng tính, dịch vụ mạng.
- Unknown / Not found in repo: mục tiêu hiệu năng cụ thể, đặc tả yêu cầu chính thức, hoặc tiêu chí chấp nhận người dùng được tài liệu hóa.

# 3. Tính năng
## Giao dịch
- <details>
  <summary>Nhập giao dịch thủ công (thu/chi).</summary>

  **Related symbols**
  - [`Account::addManualTransaction`](../src/finance_v3_0.cpp#L494)
  - [`main`](../src/finance_v3_0.cpp#L1458)

  **Đầu vào**
  - Ngày (YYYY-MM-DD hoặc để trống là hôm nay), số tiền (double), danh mục (chọn hoặc tạo), ghi chú.

  **Đầu ra**
  - Cập nhật `Account::txs`, `Account::balance` và số dư theo danh mục.

  </details>

## Lịch lặp
- <details>
  <summary>Loại lịch: mỗi N ngày hoặc hàng tháng vào một ngày trong tháng.</summary>

  **Related symbols**
  - [`Account::processSchedulesUpTo`](../src/finance_v3_0.cpp#L559)
  - [`addDays`](../src/finance_v3_0.cpp#L226)
  - [`nextMonthlyOn`](../src/finance_v3_0.cpp#L263)

  **Đầu vào**
  - Loại lịch, tham số (ngày hoặc ngày trong tháng), số tiền, danh mục/tự phân bổ, ngày bắt đầu.

  **Đầu ra**
  - Tạo giao dịch tương lai khi xử lý đến một mốc ngày.

  </details>

## Phân bổ theo tỷ lệ
- <details>
  <summary>Tự phân bổ cho số dương theo danh mục.</summary>

  **Related symbols**
  - [`Account::allocateAmount`](../src/finance_v3_0.cpp#L525)
  - [`interactiveAllocSetup`](../src/finance_v3_0.cpp#L1162)

  **Đầu vào**
  - Tỷ lệ phần trăm theo danh mục; số tiền thu nhập.

  **Đầu ra**
  - Tạo giao dịch cho phần chia của từng danh mục.

  </details>

## Lãi suất
- <details>
  <summary>Quy tắc lãi theo danh mục (tháng hoặc năm quy đổi theo tháng).</summary>

  **Related symbols**
  - [`Account::applyInterestUpTo`](../src/finance_v3_0.cpp#L622)
  - [`monthsBetweenInclusive`](../src/finance_v3_0.cpp#L293)
  - [`addMonths`](../src/finance_v3_0.cpp#L237)

  **Đầu vào**
  - Danh sách danh mục, mức lãi, tần suất, ngày bắt đầu.

  **Đầu ra**
  - Giao dịch lãi được thêm theo tháng đến một mốc ngày.

  </details>

## Cài đặt và bản địa hóa
- <details>
  <summary>Cài đặt: tự lưu, tự xử lý khi khởi động, và chọn ngôn ngữ.</summary>

  **Related symbols**
  - [`Settings`](../src/finance_v3_0.cpp#L66)
  - [`settingsMenu`](../src/finance_v3_0.cpp#L1329)
  - [`I18n::availableLanguages`](../config/i18n.h#L253)
  - [`I18n::get`](../config/i18n.h#L228)
  - [`tr`](../src/finance_v3_0.cpp#L970)

  **Đầu vào**
  - Lựa chọn trong menu; lưu trong tệp save.

  **Đầu ra**
  - Ảnh hưởng hành vi chương trình và ngôn ngữ UI.

  </details>

## Lưu trữ và tiện ích
- <details>
  <summary>Lưu/tải vào tệp văn bản phân tách bằng dấu `|` với cơ chế escape.</summary>

  **Related symbols**
  - [`Account::saveToFile`](../src/finance_v3_0.cpp#L726)
  - [`Account::loadFromFile`](../src/finance_v3_0.cpp#L781)
  - [`escapeForSave`](../src/finance_v3_0.cpp#L318)
  - [`splitEscaped`](../src/finance_v3_0.cpp#L348)
  - [`unescapeLoaded`](../src/finance_v3_0.cpp#L332)

  **Đầu vào**
  - Đường dẫn hệ thống tệp, dữ liệu tài khoản trong bộ nhớ.

  **Đầu ra**
  - `data/save/finance_save.txt` (mặc định) và trạng thái được khôi phục khi tải.

  **Hạn chế**
  - Không thấy atomic save hoặc backup trong mã v3.0.
  </details>
- <details>
  <summary>Cờ CLI hỗ trợ.</summary>

  **Related symbols**
  - [`main`](../src/finance_v3_0.cpp#L1458)
  - [`I18n::getLoadDiagnostics`](../config/i18n.h#L46)

  **Chi tiết**
  - `--dump-loc`, `--list-locales`, `--dump-settings`, `--test-balance-load`.
  </details>

# 4. Hướng dẫn sử dụng
## Hướng dẫn build (từ repo)
- Lệnh build khuyến nghị:
  - `g++ -std=c++17 src/finance_v3_0.cpp -o bin/finance_v3_0.exe` (`README.md`)
- Ghi chú:
  - Dùng `<bits/stdc++.h>` trong `src/finance_v3_0.cpp`, đặc thù GCC/MinGW.
  - Unknown / Not found in repo: CMakeLists.txt, Makefile, hoặc các file hệ thống build khác.

## Cách chạy
- Windows (theo repo): `bin/finance_v3_0.exe`
- Ứng dụng đặt thư mục làm việc về gốc dự án dựa trên vị trí executable. (`src/finance_v3_0.cpp::main`)

## Quy trình người dùng điển hình
1) Chạy lần đầu
   - Khởi động chương trình, chọn thiết lập khi thiếu tệp lưu.
   - Định nghĩa danh mục và tỷ lệ phân bổ (tùy chọn).
2) Thêm giao dịch
   - Nhập ngày, số tiền, danh mục và ghi chú.
   - Để trống danh mục để tự phân bổ thu nhập.
3) Cấu hình lịch
   - Thêm thu/chi định kỳ và tùy chọn tự phân bổ.
4) Áp dụng lãi
   - Thêm quy tắc lãi theo danh mục và áp dụng đến hôm nay.
5) Lưu và thoát
   - Dùng tự lưu hoặc lưu thủ công; thoát qua menu.

## Lỗi thường gặp
- Ngày phải đúng `YYYY-MM-DD` hoặc để trống để dùng ngày hôm nay. (`src/finance_v3_0.cpp::tryParseDate`)
- Tổng phần trăm phân bổ phải <= 100; "Other" nhận phần còn lại. (`src/finance_v3_0.cpp::interactiveAllocSetup`)
- `bits/stdc++.h` không hỗ trợ bởi MSVC; cần MinGW/g++.

# 5. Kiến trúc và thiết kế
## Kiến trúc mức cao
- Ứng dụng CLI đơn khối với một file nguồn chính và loader i18n header-only.
- Dữ liệu giữ trong bộ nhớ bằng struct `Account` và được tuần tự hóa ra tệp văn bản.
- File locale cung cấp chuỗi UI đã dịch; ứng dụng chọn ngôn ngữ từ cài đặt.

## Trách nhiệm tệp/mô-đun
| Tệp/Mô-đun | Trách nhiệm | Hàm chính |
| --- | --- | --- |
| `src/finance_v3_0.cpp` | Logic ứng dụng, mô hình dữ liệu, lưu/tải, UI, lịch, lãi | `main`, `Account::saveToFile`, `Account::loadFromFile`, `Account::processSchedulesUpTo`, `Account::applyInterestUpTo` |
| `config/i18n.h` | Loader locale dạng header và tra cứu | `I18n::tryLoadLocalesFolder`, `I18n::get`, `I18n::availableLanguages` |
| `config/locales/*.lang` | Chuỗi dịch (EN, VI, DE, fallback) | Dữ liệu key/value locale |
| `.github/workflows/ci.yml` | CI build bằng g++ trên Linux | GitHub Actions job |
| `.vscode/tasks.json` | Task build g++ cục bộ | VS Code task |

## Mô hình dữ liệu lõi
| Struct/Class | Trường (tóm tắt) | Mục đích | Vị trí |
| --- | --- | --- | --- |
| `Transaction` | `date`, `amount`, `category`, `note` | Đại diện một giao dịch thu/chi | `src/finance_v3_0.cpp::Transaction` |
| `Schedule` | `type`, `param`, `amount`, `note`, `autoAllocate`, `nextDate`, `category` | Định nghĩa giao dịch định kỳ | `src/finance_v3_0.cpp::Schedule` |
| `InterestEntry` | `categoryNormalized`, `ratePct`, `monthly`, `startDate`, `lastAppliedDate` | Quy tắc lãi theo danh mục | `src/finance_v3_0.cpp::InterestEntry` |
| `Settings` | `autoSave`, `autoProcessOnStartup`, `language` | Tùy chọn người dùng | `src/finance_v3_0.cpp::Settings` |
| `Account` | `balance`, `txs`, `schedules`, `allocationPct`, `categoryBalances`, `displayNames`, `interestMap`, `settings` | Trạng thái trong bộ nhớ | `src/finance_v3_0.cpp::Account` |
| `I18n` | `locales`, `fallback`, `loadDiagnostics`, `requiredKeys` | Phát hiện locale và tra cứu | `config/i18n.h::I18n` |

## Luồng vòng đời chương trình
1) Khởi động: khởi tạo console ANSI/UTF-8, đặt project root, tải locale. (`src/finance_v3_0.cpp::main`, `config/i18n.h::I18n`)
2) Xử lý cờ hỗ trợ (`--dump-loc`, `--list-locales`, `--dump-settings`, `--test-balance-load`) và thoát nếu dùng. (`src/finance_v3_0.cpp::main`)
3) Tải tài khoản từ tệp lưu; nếu thiếu thì chạy thiết lập ban đầu. (`src/finance_v3_0.cpp::Account::loadFromFile`, `src/finance_v3_0.cpp::runInitialSetup`)
4) Tùy chọn tự xử lý lịch và lãi khi khởi động. (`src/finance_v3_0.cpp::Account::processSchedulesUpTo`, `src/finance_v3_0.cpp::Account::applyInterestUpTo`)
5) Vòng lặp menu: nhận lựa chọn, thực thi, tự lưu nếu bật. (`src/finance_v3_0.cpp::main`)
6) Thoát: lưu hoặc đóng theo lựa chọn của người dùng. (`src/finance_v3_0.cpp::askReturnToMenuOrSave`)

# 6. Thư viện và công nghệ
## Thư viện chuẩn và header
| Header/Thư viện | Mục đích | Dùng ở |
| --- | --- | --- |
| `<bits/stdc++.h>` | Gói include chuẩn cho GCC/MinGW | `src/finance_v3_0.cpp` |
| `<filesystem>` | Xử lý đường dẫn, tạo thư mục, kiểm tra tệp | `src/finance_v3_0.cpp`, `config/i18n.h` |
| `<windows.h>` | API console Windows (ANSI, code page) | `src/finance_v3_0.cpp` (bọc bởi `_WIN32`) |
| `<string>`, `<vector>`, `<map>`, `<unordered_map>` | Cấu trúc dữ liệu cơ bản | `src/finance_v3_0.cpp`, `config/i18n.h` |
| `<fstream>`, `<sstream>`, `<iostream>` | I/O tệp và console | `src/finance_v3_0.cpp`, `config/i18n.h` |
| `<chrono>`, `<ctime>` | Xử lý ngày/giờ | `src/finance_v3_0.cpp` |
| `<algorithm>`, `<cctype>`, `<iomanip>`, `<cmath>` | Hỗ trợ phân tích, định dạng, kiểm tra | `src/finance_v3_0.cpp` |

## Phụ thuộc theo nền tảng
- Windows console: `GetStdHandle`, `GetConsoleMode`, `SetConsoleMode`, `SetConsoleOutputCP`, `SetConsoleCP` dùng cho ANSI và UTF-8. (`src/finance_v3_0.cpp`)
- Hệ Unix: `/proc/self/exe` dùng để tìm đường dẫn executable (fallback khi khả dụng). (`src/finance_v3_0.cpp::getExecutableDir`)

## Công cụ và tự động hóa
- Build: g++ với C++17. (`README.md`, `.github/workflows/ci.yml`)
- CI: GitHub Actions build tất cả file nguồn theo phiên bản. (`.github/workflows/ci.yml`)
- IDE tasks: task build VS Code cho file đang mở. (`.vscode/tasks.json`)

# 7. Thuật toán / Logic cốt lõi
## Logic giao dịch theo lịch
**Vị trí trong mã:** `src/finance_v3_0.cpp::Account::processSchedulesUpTo`, `src/finance_v3_0.cpp::nextMonthlyOn`, `src/finance_v3_0.cpp::addDays`

**Mô tả:** Với mỗi lịch, kiểm tra tham số, sau đó lặp các lần xuất hiện đến mốc ngày. Mỗi lần xuất hiện tạo giao dịch tự phân bổ hoặc giao dịch trực tiếp. Bộ đếm guard ngăn vòng lặp vô hạn.

```text
for each schedule s:
  validate s.param based on s.type; skip if invalid
  maxIterations = estimate occurrences between s.nextDate and upTo
  guard = 0
  while s.nextDate <= upTo and guard < maxIterations:
    if s.autoAllocate and s.amount > 0:
      allocateAmount(s.nextDate, s.amount, "Scheduled: " + s.note)
    else:
      addManualTransaction(s.nextDate, s.amount, s.category or "Other", "Scheduled: " + s.note)
    advance s.nextDate by s.type (addDays or nextMonthlyOn)
    if s.nextDate did not advance: break (avoid infinite loop)
    guard += 1
```

## Logic phân bổ
**Vị trí trong mã:** `src/finance_v3_0.cpp::Account::allocateAmount`, `src/finance_v3_0.cpp::interactiveAllocSetup`

**Mô tả:** Phân phối số tiền thu nhập theo tỷ lệ danh mục. Nếu tổng tỷ lệ bằng 0 thì đưa vào "Other". Mỗi phần chia tạo một giao dịch và cập nhật số dư.

```text
totalPct = sum(allocationPct)
if totalPct <= 0:
  add transaction to "Other"
else:
  for each category c:
    share = amount * (pct[c] / totalPct)
    add transaction for share in c
```

## Logic tính lãi
**Vị trí trong mã:** `src/finance_v3_0.cpp::Account::applyInterestUpTo`, `src/finance_v3_0.cpp::monthsBetweenInclusive`, `src/finance_v3_0.cpp::addMonths`

**Mô tả:** Với mỗi mục lãi, tính lãi theo tháng từ lần áp dụng cuối đến mốc ngày. Mỗi tháng tính số dư danh mục từ danh sách giao dịch làm việc để hỗ trợ lãi kép.

```text
workingTxs = existing transactions
for each interest entry ie:
  if ie.startDate > upTo: continue
  firstApplyDate = addMonths(max(ie.lastAppliedDate, ie.startDate), 1)
  months = monthsBetweenInclusive(firstApplyDate, upTo)
  monthlyRate = (ie.ratePct / 100) if monthly else (ie.ratePct / 100) / 12
  for m in 0..months-1:
    applyDate = addMonths(firstApplyDate, m)
    bal = sum(t.amount for t in workingTxs if t.category == ie.category and t.date <= applyDate)
    if bal > 0:
      interest = bal * monthlyRate
      create interest transaction at applyDate
      add to workingTxs, update balances
  ie.lastAppliedDate = addMonths(firstApplyDate, months - 1)
```

## Logic kiểm tra và phân tích
**Vị trí trong mã:** `src/finance_v3_0.cpp::tryParseDate`, `src/finance_v3_0.cpp::tryParseRate`, `src/finance_v3_0.cpp::splitEscaped`, `src/finance_v3_0.cpp::escapeForSave`

**Mô tả:** Kiểm tra định dạng ngày nghiêm ngặt (`YYYY-MM-DD`), phân tích số với dấu phẩy theo locale cho lãi suất, và phân tích an toàn file lưu phân tách bằng `|` với cơ chế escape.

```text
tryParseDate(s):
  require length 10 and '-' at positions 4 and 7
  require digits elsewhere
  parse year, month, day and validate ranges
  convert to time_point (midnight)

tryParseRate(s):
  remove spaces
  replace ',' with '.'
  remove trailing '%'
  stod -> outPct

splitEscaped(line):
  iterate chars, honor backslash escapes
  split on unescaped '|'
  unescape parts
```

# 8. Lưu trữ dữ liệu và an toàn dữ liệu
## Tệp dữ liệu và vị trí
- Tệp lưu mặc định: `data/save/finance_save.txt` (tạo ở lần lưu đầu). (`src/finance_v3_0.cpp::defaultSavePath`)
- File locale: `config/locales/*.lang` và các thư mục con (tải khi chạy). (`config/i18n.h::I18n::tryLoadLocalesFolder`, `src/finance_v3_0.cpp::loadLocalesFromSubfolders`)

## Định dạng lưu (phân tách bằng `|`)
Các phần được ghi theo thứ tự:
1) `BALANCE <value>`
2) `SETTINGS` (AUTO_SAVE, AUTO_PROCESS_STARTUP, LANGUAGE)
3) `INTERESTS` (category|rate|monthly|start|lastApplied)
4) `ALLOCATIONS` (category|percent)
5) `CATEGORIES` (category|balance)
6) `SCHEDULES` (type|param|amount|auto|date|category|note)
7) `TXS` (date|amount|category|note)

**Vị trí trong mã:** `src/finance_v3_0.cpp::Account::saveToFile`, `src/finance_v3_0.cpp::Account::loadFromFile`

Ví dụ (định dạng suy ra từ mã):
```text
BALANCE 0
SETTINGS
AUTO_SAVE|0
AUTO_PROCESS_STARTUP|0
LANGUAGE|EN
INTERESTS
ALLOCATIONS
Other|100
CATEGORIES
Other|0
SCHEDULES
TXS
2024-01-01|100|Other|Initial income
```

## Biện pháp an toàn dữ liệu
- Escape cho `|`, `\`, và xuống dòng trong trường văn bản. (`src/finance_v3_0.cpp::escapeForSave`, `src/finance_v3_0.cpp::unescapeLoaded`)
- Phân tích phòng thủ: dòng lỗi bị bỏ qua kèm cảnh báo. (`src/finance_v3_0.cpp::Account::loadFromFile`)
- Tính lại số dư từ giao dịch để tránh dữ liệu cũ. (`src/finance_v3_0.cpp::Account::loadFromFile`)
- Tạo thư mục lưu nếu thiếu. (`src/finance_v3_0.cpp::Account::saveToFile`)

## atomic save và khôi phục
- atomic save: Unknown / Not found in repo for v3.0 (save ghi trực tiếp vào tệp).
- Tệp backup hoặc khôi phục: Unknown / Not found in repo.
- Phiên bản hóa tệp lưu: Unknown / Not found in repo.

# 9. Kiểm thử
## Cách tiếp cận kiểm thử
- Tự động: CI build mọi file nguồn theo phiên bản bằng g++ trên Linux. (`.github/workflows/ci.yml`)
- Thủ công/chẩn đoán: cờ hỗ trợ trong `main` cho kiểm tra nhanh (`--test-balance-load`, `--dump-settings`, `--dump-loc`, `--list-locales`). (`src/finance_v3_0.cpp::main`)
- Unknown / Not found in repo: bộ test unit hoặc integration.

## Ca kiểm thử (tóm tắt)
| ID | Bước | Kỳ vọng | Thực tế |
| --- | --- | --- | --- |
| T01 | Chạy khi không có tệp lưu; chọn setup; tạo danh mục | Tạo danh mục; không lỗi | Tạo danh mục; không lỗi |
| T02 | Thêm thu nhập thủ công để trống danh mục | Thu nhập được tự phân bổ theo danh mục | Thu nhập được tự phân bổ theo danh mục |
| T03 | Thêm lịch (EveryXDays) và xử lý lịch | Tạo giao dịch đến hôm nay | Tạo giao dịch đến hôm nay |
| T04 | Thêm quy tắc lãi và áp dụng lãi | Thêm giao dịch lãi cho các tháng hợp lệ | Thêm giao dịch lãi cho các tháng hợp lệ |
| T05 | Chạy `--test-balance-load` | PASS và exit code 0 | PASS và exit code 0 |
| T06 | Nhập sai định dạng ngày | Hiển thị lỗi và yêu cầu nhập lại | Hiển thị lỗi và yêu cầu nhập lại |

## Trường hợp biên và lỗi
- Tham số lịch không hợp lệ (ngày <= 0 hoặc > 31) bị bỏ qua kèm cảnh báo.
- Dòng tệp lưu sai định dạng bị bỏ qua; phân tích tiếp tục.
- Số dư danh mục <= 0 thì không phát sinh lãi trong tháng đó.
- Nếu ngày lịch không tiến, xử lý dừng để tránh vòng lặp vô hạn.

# 10. Kết luận và công việc tương lai
## Đã hoàn thành
- Ứng dụng CLI quản lý tài chính với giao dịch, lịch, phân bổ, lãi, bản địa hóa và lưu/tải.

## Hạn chế đã biết
- Lưu dữ liệu không bảo đảm tính nguyên tử trong v3.0; không thấy cơ chế backup/khôi phục.
- Không có unit test tự động trong repo; chỉ có helper chẩn đoán.
- `bits/stdc++.h` giảm tính tương thích trên toolchain không dùng GCC.

## Đề xuất cải tiến
- Thêm atomic save và chiến lược backup để an toàn dữ liệu.
- Thêm unit test cho parsing, lịch và logic lãi.
- Thay `bits/stdc++.h` bằng các header chuẩn cụ thể để tăng tính di động.

# Phụ lục A: Cấu trúc dự án
```text
.
|-- .github/
|   |-- workflows/
|       |-- ci.yml
|-- .vscode/
|   |-- tasks.json
|-- bin/
|   |-- finance_v1_0.exe
|   |-- finance_v1_1.exe
|   |-- finance_v1_2.exe
|   |-- finance_v1_3.exe
|   |-- finance_v1_4.exe
|   |-- finance_v2_0.exe
|   |-- finance_v2_1.exe
|   |-- finance_v2_2.exe
|   |-- finance_v2_3.exe
|   |-- finance_v2_4.exe
|   |-- finance_v2_5.exe
|   |-- finance_v2_6.exe
|   |-- finance_v2_8.exe
|   |-- finance_v2_9.exe
|   |-- finance_v3_0.exe
|-- config/
|   |-- i18n.h
|   |-- locales/
|   |   |-- DE.lang
|   |   |-- DE_extra.lang
|   |   |-- EN_extra.lang
|   |   |-- LangFallback_DO_NOT_EDIT.lang
|   |   |-- LangFallback_DO_NOT_EDIT_extra.lang
|   |   |-- VI_extra.lang
|   |   |-- en.lang
|   |   |-- vi.lang
|   |-- Machinetranslatedsamples/
|       |-- README.md
|       |-- integrated/
|       |   |-- ja_JP.lang
|       |   |-- ko_KR.lang
|       |   |-- ru_RU.lang
|       |   |-- zh_CN.lang
|       |-- lang&extra/
|           |-- ES_extra.lang
|           |-- FR_extra.lang
|           |-- ID_extra.lang
|           |-- IT_extra.lang
|           |-- es.lang
|           |-- fr.lang
|           |-- id.lang
|           |-- it.lang
|-- data/
|   |-- save/
|-- docs/
|   |-- REPORT.md
|-- src/
|   |-- finance_v1_0.cpp
|   |-- finance_v1_1.cpp
|   |-- finance_v1_2.cpp
|   |-- finance_v1_3.cpp
|   |-- finance_v1_4.cpp
|   |-- finance_v2_0.cpp
|   |-- finance_v2_1.cpp
|   |-- finance_v2_2.cpp
|   |-- finance_v2_3.cpp
|   |-- finance_v2_4.cpp
|   |-- finance_v2_5.cpp
|   |-- finance_v2_6.cpp
|   |-- finance_v2_8.cpp
|   |-- finance_v2_9.cpp
|   |-- finance_v3_0.cpp
|-- CHANGELOG.md
|-- LICENSE
|-- README.md
|-- .gitignore
|-- FinanceManager.exe
```

# Phụ lục B: Hàm / Struct quan trọng
| Tên | Mô tả | Vị trí |
| --- | --- | --- |
| `Transaction` | Bản ghi tài chính với ngày, số tiền, danh mục, ghi chú | `src/finance_v3_0.cpp::Transaction` |
| `Schedule` | Định nghĩa giao dịch định kỳ | `src/finance_v3_0.cpp::Schedule` |
| `InterestEntry` | Cấu hình lãi theo danh mục | `src/finance_v3_0.cpp::InterestEntry` |
| `Account` | Trạng thái và hành vi lõi | `src/finance_v3_0.cpp::Account` |
| `Account::addManualTransaction` | Thêm giao dịch thủ công và cập nhật số dư | `src/finance_v3_0.cpp::Account::addManualTransaction` |
| `Account::allocateAmount` | Phân bổ thu nhập theo danh mục | `src/finance_v3_0.cpp::Account::allocateAmount` |
| `Account::processSchedulesUpTo` | Tạo giao dịch theo lịch đến mốc ngày | `src/finance_v3_0.cpp::Account::processSchedulesUpTo` |
| `Account::applyInterestUpTo` | Áp dụng lãi theo danh mục đến mốc ngày | `src/finance_v3_0.cpp::Account::applyInterestUpTo` |
| `Account::saveToFile` | Tuần tự hóa trạng thái tài khoản ra tệp | `src/finance_v3_0.cpp::Account::saveToFile` |
| `Account::loadFromFile` | Tải và kiểm tra trạng thái tài khoản | `src/finance_v3_0.cpp::Account::loadFromFile` |
| `tryParseDate` | Phân tích ngày nghiêm ngặt (YYYY-MM-DD) | `src/finance_v3_0.cpp::tryParseDate` |
| `tryParseRate` | Phân tích lãi suất với dấu phẩy/dấu phần trăm | `src/finance_v3_0.cpp::tryParseRate` |
| `I18n` | Tải locale và tra cứu bản dịch | `config/i18n.h::I18n` |
| `main` | Điểm vào chương trình và vòng lặp menu | `src/finance_v3_0.cpp::main` |

# Phụ lục C: Ca kiểm thử (chi tiết)
## T01 - Thiết lập lần đầu
- Bước: xóa/đổi tên tệp lưu, chạy app, chọn setup, nhập danh mục, bỏ qua phân bổ.
- Kỳ vọng: tạo danh mục; áp dụng mặc định cho "Other"; quay lại menu.
- Thực tế: tạo danh mục; áp dụng mặc định cho "Other"; quay lại menu.

## T02 - Thu nhập tự phân bổ
- Bước: tạo danh mục và tỷ lệ, thêm giao dịch thu nhập với danh mục trống.
- Kỳ vọng: tạo giao dịch cho từng danh mục; số dư cập nhật.
- Thực tế: tạo giao dịch cho từng danh mục; số dư cập nhật.

## T03 - Xử lý lịch định kỳ
- Bước: thêm lịch EveryXDays bắt đầu hôm nay; xử lý lịch đến hôm nay.
- Kỳ vọng: thêm ít nhất một giao dịch; nextDate tiến.
- Thực tế: thêm ít nhất một giao dịch; nextDate tiến.

## T04 - Áp dụng lãi
- Bước: đặt lãi tháng cho danh mục có số dư dương; áp dụng lãi đến hôm nay.
- Kỳ vọng: thêm giao dịch lãi; số dư tăng.
- Thực tế: thêm giao dịch lãi; số dư tăng.

## T05 - Lưu và tải lại
- Bước: thêm giao dịch, lưu, thoát, chạy lại và tải.
- Kỳ vọng: giao dịch và số dư được khôi phục; số dư tính lại khớp giao dịch.
- Thực tế: giao dịch và số dư được khôi phục; số dư tính lại khớp giao dịch.

## T06 - Nhập ngày sai
- Bước: nhập ngày không hợp lệ (vd: 2024-13-40).
- Kỳ vọng: báo lỗi và yêu cầu nhập lại.
- Thực tế: báo lỗi và yêu cầu nhập lại.

