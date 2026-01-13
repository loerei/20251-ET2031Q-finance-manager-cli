# Phân công công việc, tiến độ dự án Finance Manager

Tài liệu này tổng hợp phân công và mốc thời gian triển khai cho dự án Finance Manager trong giai đoạn 2025-11-11 đến 2026-01-12. Kế hoạch ưu tiên theo tuần và có các mốc kiểm soát chất lượng/phát hành.

## Members
- Bùi&nbsp;Thanh&nbsp;Phong (Nhóm Trưởng)
- Nguyễn&nbsp;Bảo&nbsp;Nhung
- Phạm&nbsp;Ngọc&nbsp;Khiêm
- Nguyễn&nbsp;Trí&nbsp;Hùng
- Trịnh&nbsp;Quốc&nbsp;Hùng
- Nguyễn&nbsp;Phạm&nbsp;Tiến&nbsp;Hưng

## Timeline
| Week | Date range | Focus |
| --- | --- | --- |
| W1 | 2025-11-11 - 2025-11-17 | Khởi động, chốt phạm vi, rà soát kiến trúc |
| W2 | 2025-11-18 - 2025-11-24 | Mô hình dữ liệu, giao dịch thủ công |
| W3 | 2025-11-25 - 2025-12-01 | Lịch định kỳ |
| W4 | 2025-12-02 - 2025-12-08 | Phân bổ danh mục, lãi suất |
| W5 | 2025-12-09 - 2025-12-15 | Lưu/tải, `i18n`, chuẩn hóa đường dẫn |
| W6 | 2025-12-16 - 2025-12-22 | CLI menu, tài liệu, đóng băng tài liệu |
| W7 | 2025-12-23 - 2025-12-29 | Kiểm thử, `CI` |
| W8 | 2025-12-30 - 2026-01-05 | `Release candidate` và hồi quy |
| W9 | 2026-01-06 - 2026-01-12 | Phát hành cuối |

Milestones:
- <span style='color: #2563eb; font-weight: 700;'>M1</span> (2025-11-17): Chốt phạm vi, kiến trúc
- <span style='color: #2563eb; font-weight: 700;'>M2</span> (2025-12-15): `Feature freeze`
- <span style='color: #2563eb; font-weight: 700;'>M3</span> (2025-12-22): `Doc freeze`
- <span style='color: #2563eb; font-weight: 700;'>M4</span> (2025-12-29): Hoàn tất kiểm thử
- <span style='color: #2563eb; font-weight: 700;'>M5</span> (2026-01-05): `Release candidate`
- <span style='color: #2563eb; font-weight: 700;'>M6</span> (2026-01-12): Phát hành cuối

## Core deliverables
| ID | Workstream | Task title | Scope | Deliverable (file/link) | Deadline | Status | Priority | Effort | Dependencies | Definition of Done | Owner | Contributors | Reviewed by |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C01 | Phạm vi, tài liệu | Đặc tả phạm vi và kiến trúc | Tổng hợp yêu cầu trong/ngoài phạm vi | `docs/REPORT_VI.md` | 2025-11-17 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #c9a400; font-weight: 700;'>M</span> | <span style="opacity: 0.45;">--</span> | Mục phạm vi, kiến trúc, luồng chính rõ ràng | Nguyễn&nbsp;Bảo&nbsp;Nhung,<br>Bùi&nbsp;Thanh&nbsp;Phong | All | All |
| C02 | <span style='color: #0f766e; font-weight: 600;'>Thiết kế</span> - Flowchart | Quyết định, thiết kế flowchart | Luồng nghiệp vụ, menu, tương tác chính | [Miro Flowchart](https://miro.com/app/board/uXjVGd4fsqQ=/?share_link_id=714558277659) | 2025-11-17 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #7c3aed; font-weight: 700;'>E</span> | C01 | Flowchart phản ánh đầy đủ luồng chính và đồng bộ với CLI | Nguyễn&nbsp;Trí&nbsp;Hùng,<br>Phạm&nbsp;Ngọc&nbsp;Khiêm,<br>Nguyễn&nbsp;Bảo&nbsp;Nhung,<br>Bùi&nbsp;Thanh&nbsp;Phong | All | <span style="opacity: 0.45;">--</span> |
| C03 | <span style='color: #2563eb; font-weight: 600;'>Coding</span> - Core model | Định nghĩa mô hình dữ liệu | `Transaction`, `Schedule`, `InterestEntry`, `Settings`, `Account` | `src/finance_v3_0.cpp::Transaction`, `src/finance_v3_0.cpp::Schedule`, `src/finance_v3_0.cpp::InterestEntry`, `src/finance_v3_0.cpp::Settings`, `src/finance_v3_0.cpp::Account` | 2025-11-24 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #dc2626; font-weight: 700;'>H</span> | C01 | Struct và trạng thái tài khoản được định nghĩa đầy đủ | Trịnh&nbsp;Quốc&nbsp;Hùng,<br>Phạm&nbsp;Ngọc&nbsp;Khiêm,<br>Bùi&nbsp;Thanh&nbsp;Phong | All | <span style="opacity: 0.45;">--</span> |
| C04 | <span style='color: #2563eb; font-weight: 600;'>Coding</span> - Giao dịch | Thêm giao dịch thủ công | Thu/chi, danh mục, ghi chú | `src/finance_v3_0.cpp::Account::addManualTransaction`, `src/finance_v3_0.cpp::tryParseDate` | 2025-11-24 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #dc2626; font-weight: 700;'>H</span> | C03 | Thêm giao dịch cập nhật `balance`, lưu `category` và `note` | Trịnh&nbsp;Quốc&nbsp;Hùng,<br>Bùi&nbsp;Thanh&nbsp;Phong | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> |
| C05 | <span style='color: #2563eb; font-weight: 600;'>Coding</span> - Lịch định kỳ | Xử lý giao dịch theo lịch | `EveryXDays`, `MonthlyOn` | `src/finance_v3_0.cpp::Account::processSchedulesUpTo`, `src/finance_v3_0.cpp::addDays`, `src/finance_v3_0.cpp::nextMonthlyOn` | 2025-12-01 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #dc2626; font-weight: 700;'>H</span> | C03 | Tạo giao dịch đến mốc ngày, có guard vòng lặp | Phạm&nbsp;Ngọc&nbsp;Khiêm,<br>Bùi&nbsp;Thanh&nbsp;Phong | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> |
| C06 | <span style='color: #2563eb; font-weight: 600;'>Coding</span> - Phân bổ | Phân bổ thu nhập theo % | Danh mục và mục "Other" | `src/finance_v3_0.cpp::Account::allocateAmount`, `src/finance_v3_0.cpp::interactiveAllocSetup` | 2025-12-08 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #c9a400; font-weight: 700;'>M</span> | C04 | Tổng % hợp lệ, "Other" nhận phần còn lại | Bùi&nbsp;Thanh&nbsp;Phong,<br>Nguyễn&nbsp;Trí&nbsp;Hùng | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> |
| C07 | <span style='color: #2563eb; font-weight: 600;'>Coding</span> - Lãi suất | Áp dụng lãi theo danh mục | Tháng/năm quy đổi tháng | `src/finance_v3_0.cpp::Account::applyInterestUpTo`, `src/finance_v3_0.cpp::monthsBetweenInclusive`, `src/finance_v3_0.cpp::addMonths` | 2025-12-08 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #7c3aed; font-weight: 700;'>E</span> | C04 | Tính lãi đúng theo mốc thời gian | Bùi&nbsp;Thanh&nbsp;Phong,<br>Nguyễn&nbsp;Trí&nbsp;Hùng | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> |
| C08 | <span style='color: #2563eb; font-weight: 600;'>Coding</span> - Lưu trữ | Lưu/tải và an toàn dữ liệu | `escape`, phân tách file, đường dẫn chạy độc lập | `src/finance_v3_0.cpp::Account::saveToFile`, `src/finance_v3_0.cpp::Account::loadFromFile`, `src/finance_v3_0.cpp::escapeForSave`, `src/finance_v3_0.cpp::splitEscaped`, `src/finance_v3_0.cpp::unescapeLoaded`, `data/save/finance_save.txt` | 2025-12-15 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #dc2626; font-weight: 700;'>H</span> | C03 | Lưu/tải ổn, chạy được từ mọi thư mục làm việc | Nguyễn&nbsp;Phạm&nbsp;Tiến&nbsp;Hưng,<br>Bùi&nbsp;Thanh&nbsp;Phong | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> |
| C09 | <span style='color: #2563eb; font-weight: 600;'>Coding</span> - `i18n` | Loader và file locale | Quét `config/locales/` và fallback | `config/i18n.h`, `config/localeslocales/X.lang` | 2025-12-15 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #7c3aed; font-weight: 700;'>E</span> | C03 | `.lang` load ổn, key fallback hoạt động | Bùi&nbsp;Thanh&nbsp;Phong | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> |
| C10 | <span style='color: #2563eb; font-weight: 600;'>Coding</span> - CLI/UX | Menu, cài đặt, helper flags | `auto-save`, `auto-process`, `language`, `--list-locales`... | `src/finance_v3_0.cpp` | 2025-12-22 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #7c3aed; font-weight: 700;'>E</span> | C04, C05, C06, C07, C08, C09 | Menu đầy đủ, helper flags chạy được | Nguyễn&nbsp;Bảo&nbsp;Nhung | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> |
| C11 | Tài liệu sử dụng | Hướng dẫn build/run | Lệnh `g++`, cách chạy, mô tả flags | `README.md` | 2025-12-22 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #dc2626; font-weight: 700;'>H</span> | C10 | Tài liệu build/run cập nhật và chính xác | Bùi&nbsp;Thanh&nbsp;Phong | <span style="opacity: 0.45;">--</span> | All |
| C12 | CI | Xây dựng trên GitHub Actions | Build `g++` trên Linux | `.github/workflows/ci.yml` | 2025-12-29 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P1 | <span style='color: #1b8f2e; font-weight: 700;'>L</span> | C11 | Pipeline build pass trên `main` | Bùi&nbsp;Thanh&nbsp;Phong | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> |
| C13 | Kiểm thử | Bộ ca kiểm thử thủ công | Danh sách test T01-T06 | `docs/REPORT_VI.md` | 2025-12-29 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P1 | <span style='color: #c9a400; font-weight: 700;'>M</span> | C10 | Ca kiểm thử rõ bước/kỳ vọng | Nguyễn&nbsp;Trí&nbsp;Hùng,<br>Phạm&nbsp;Ngọc&nbsp;Khiêm | All | All |
| C14 | Phát hành | Đóng gói bản RC | Tạo binary RC | `bin/finance_v3_0.exe` | 2026-01-05 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P1 | <span style='color: #1b8f2e; font-weight: 700;'>L</span> | C12, C13 | RC build chạy được và dung lượng ổn | All | All | All |
| C15 | Phát hành | Chốt phiên bản cuối | Cập nhật lịch sử thay đổi | `CHANGELOG.md` | 2026-01-12 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P0 | <span style='color: #1b8f2e; font-weight: 700;'>L</span> | C14 | `CHANGELOG.md` có mục phát hành cuối | Phạm&nbsp;Ngọc&nbsp;Khiêm | Bùi&nbsp;Thanh&nbsp;Phong | All |

## Optional dev tooling
| ID | Workstream | Task title | Scope | Deliverable (file/link) | Deadline | Status | Priority | Effort | Dependencies | Definition of Done | Owner | Contributors | Reviewed by |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| O01 | IDE tooling | VS Code build task | Task build `g++` cho `finance_v3_0.cpp` | `.vscode/tasks.json` | 2025-11-24 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P2 | <span style='color: #1b8f2e; font-weight: 700;'>L</span> | C11 | `Ctrl+Shift+B` build thành công | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> |
| O02 | Build tooling | CMake build file | Hỗ trợ build ngoài `g++` thủ công | `CMakeLists.txt` | 2025-12-08 | <span style="color: #c9a400; font-weight: 600;">Planned</span> | P3 | <span style='color: #dc2626; font-weight: 700;'>H</span> | C03 | `cmake --build` chạy được | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> | <span style="opacity: 0.45;">--</span> |
| O03 | Test tooling | Script smoke test | Chạy `--test-balance-load`, `--list-locales` | `tests/run_smoke.ps1` | 2025-12-29 | <span style="color: #1b8f2e; font-weight: 600;">Finished</span> | P2 | <span style='color: #1b8f2e; font-weight: 700;'>L</span> | C10 | Script exit code 0 | Phạm&nbsp;Ngọc&nbsp;Khiêm | Nguyễn&nbsp;Trí&nbsp;Hùng,<br>Bùi&nbsp;Thanh&nbsp;Phong | <span style="opacity: 0.45;">--</span> |
