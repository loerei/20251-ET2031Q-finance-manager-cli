# Finance Manager - Complete Changelog

**Last Updated:** February 2026

---

## Version 1.0 (Foundation)
**File:** `finance_v1_0.cpp` (449 lines)  
**Status:** Initial Release

### Core Features Implemented:
- **Transaction Management** (Lines 18-23)
  - Manual transaction tracking with date, amount, category, note
  - Positive amounts = income, negative = expenses
  
- **Schedule System** (Lines 25-32)
  - Two schedule types: EveryXDays, MonthlyDay
  - Repeating transactions with auto-allocation option
  
- **Category System**
  - Basic category tracking
  - Auto-allocation by percentage across categories
  
- **Interest Calculation** (Lines 35+)
  - Basic saving interest calculation
  
- **Persistence** (Lines 40+)
  - Save/Load from "finance_save.txt"
  
- **Date Handling**
  - Date parsing and formatting (YYYY-MM-DD)
  - Basic chrono integration

### Key Functions:
- `parseDate()` - Parse YYYY-MM-DD format
- `toDateString()` - Format date to string
- Basic transaction & schedule management

---

## Version 1.1 (Thread Safety & Escaping)
**File:** `finance_v1_1.cpp`
**Status:** Enhanced baseline

### Changes from 1.0:
**New/Improved Features:**

1. **Safe Localtime** (Lines 36-47)
   - Platform-specific thread-safe localtime implementation
   - Support for Windows/MinGW, POSIX, macOS, Linux
   - Replaces unsafe `localtime()` calls

2. **Escaping Mechanism** (Lines ~200-250)
   - New: `escapeForSave()` - Escape special characters (\\, |, \n)
   - New: `unescapeLoaded()` - Restore escaped characters
   - New: `splitEscaped()` - Parse pipe-delimited data with escape handling
   - Prevents corruption of pipe-delimited save file format

3. **Robust Save/Load**
   - Implements pipe-delimited format with proper escaping
   - Handles notes and categories with special characters

### Added Functions:
- `safeLocaltime()` - Thread-safe time handling
- `escapeForSave()` - Character escaping
- `unescapeLoaded()` - Character unescaping
- `splitEscaped()` - Safe field parsing

---

## Version 1.2 (UI Refinement)
**File:** `finance_v1_2.cpp`
**Status:** UX Enhancement

### Changes from 1.1:
- Added `askReturnToMenu()` flow so users can choose to continue or exit after actions.
- More consistent input validation for dates, amounts, and schedule parameters.
- Menu copy and prompts tightened for clearer guidance.

---

## Version 1.3 (Allocation Date Fix)
**File:** `finance_v1_3.cpp`
**Status:** Data Accuracy Fixes

### Changes from 1.2:
- Auto-allocation now records the correct transaction date for both manual and scheduled income.
- Scheduled auto-allocation uses the schedule date instead of "today".
- Minor prompt and flow cleanups.

---

## Version 1.4 (Category Normalization)
**File:** `finance_v1_4.cpp`
**Status:** Category Model Upgrade

### Changes from 1.3:
- Normalized category keys with preserved display names to prevent duplicates from casing/spacing.
- Schedules now carry an optional category (supports recurring expenses with explicit categories).
- Category selection supports name or number, with auto-allocate only for positive amounts.
- Category balances are recomputed from transactions on load for consistency.

---

## Version 2.0 (Guided Setup & Allocation Control)
**File:** `finance_v2_0.cpp`
**Status:** Feature Expansion

### Changes from 1.4:
- Added guided category setup and allocation setup (Other is treated as remainder).
- Improved category list handling with display-name sorting and normalization.
- Allocation editing now operates against existing categories for safer updates.

---

## Version 2.1 (Per-Category Interest)
**File:** `finance_v2_1.cpp`
**Status:** Financial Logic Expansion

### Changes from 2.0:
- Introduced per-category interest entries with monthly or annual rates and start dates.
- Added month-based interest processing with compounding logic.
- New interest menu to add/update/remove rates and apply interest on demand.

---

## Version 2.2 (Settings + EN/VI Localization)
**File:** `finance_v2_2.cpp`
**Status:** UX & Settings

### Changes from 2.1:
- Added settings for auto-save, auto-process on startup, and language preference.
- Introduced EN/VI translation helper for menu and prompt strings.
- Settings persisted in save file and applied on startup (auto-process).
- Auto-save integrated into exit/return-to-menu flows.

---

## Version 2.3 (Atomic Save + Operation Log)
**File:** `finance_v2_3.cpp`
**Status:** Reliability & Auditing

### Changes from 2.2:
- Atomic save via temp file + rename to reduce corruption risk.
- Added append-only operation log (`finance_full_log.txt`) for audit/debugging.
- Transaction merging and snapshots to avoid duplicates and support safer schedule/interest processing.

---

## Version 2.4 (Portable Paths + External i18n)
**File:** `finance_v2_4.cpp`
**Status:** Localization & Portability

### Changes from 2.3:
- Switched to external i18n loader (`config/i18n.h`) with dynamic language list.
- Portable working-directory resolution based on executable location.
- Added helper flags: `--dump-loc`, `--list-locales`, `--dump-settings`.

---

## Version 2.5 (Refactor & Input Parsing)
**File:** `finance_v2_5.cpp`
**Status:** Refactor

### Changes from 2.4:
- Reorganized code into focused helper sections (date, parsing, categories, allocations, schedules).
- Improved numeric input parsing and validation helpers.
- Cleaner menu/config flows using shared utilities.

---

## Version 2.6 (ANSI Console UI)
**File:** `finance_v2_6.cpp`
**Status:** UI Polish

### Changes from 2.5:
- Added ANSI terminal helpers for clear-screen and cursor control.
- Enabled ANSI escape sequences on Windows consoles.
- Menu rendering updated to use the terminal helpers.

---

## Version 2.8 (Locale Improvements)
**File:** `finance_v2_8.cpp`
**Status:** i18n Enhancements

### Changes from 2.6:
- Added DE to supported languages in i18n data.
- Interest rate parsing now accepts comma decimal format (e.g., "0,5%").
- Expanded localized strings for menu and settings flows.

---

## Version 2.9 (Balance Recompute Fixes)
**File:** `finance_v2_9.cpp`
**Status:** Data Correctness

### Changes from 2.8:
- Recompute balance from transactions on load when txs exist, avoiding stale saved balance.
- Added `--test-balance-load` regression helper.
- Additional safeguards for category balance recomputation.

---

## Version 3.0 (Locale Pack Discovery)
**File:** `finance_v3_0.cpp`
**Status:** Latest Release

### Changes from 2.9:
- Scans locale subfolders under `config/locales` and `locales` for additional packs.
- Improved UTF-8 handling on Windows (console code page + locale setup).
- Retains helper flags and settings flow with expanded i18n coverage.


