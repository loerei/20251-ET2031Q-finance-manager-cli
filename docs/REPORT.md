# 1. Introduction
- Project overview: a C++17 console application for personal finance tracking with transactions, recurring schedules, category allocation, and per-category interest. Primary implementation is `src/finance_v3_0.cpp`.
- Goals: allow users to record income/expenses, auto-allocate income by category percentage, process scheduled transactions, and apply interest; persist data to a local save file.
- Target users: individuals managing personal finances in a terminal/CLI environment.
- High-level feature summary : manual transactions, recurring schedules, allocation rules, interest rules, settings (auto-save, auto-process on startup, language), file persistence, and multi-language UI via locale files.

# 2. Requirements and Scope
## Functional requirements 
- Record manual transactions with date, amount, category, and note. (`src/finance_v3_0.cpp::Account::addManualTransaction`)
- Define recurring schedules (every N days or monthly on a specific day) and process them up to a date. (`src/finance_v3_0.cpp::Account::processSchedulesUpTo`)
- Allocate income across categories by percentage, with "Other" as remainder. (`src/finance_v3_0.cpp::Account::allocateAmount`, `src/finance_v3_0.cpp::interactiveAllocSetup`)
- Configure per-category interest (monthly or annual converted to monthly) and apply it over time. (`src/finance_v3_0.cpp::Account::applyInterestUpTo`)
- Persist and reload all account data from a local file. (`src/finance_v3_0.cpp::Account::saveToFile`, `src/finance_v3_0.cpp::Account::loadFromFile`)
- Provide a menu-driven CLI with settings and localization. (`src/finance_v3_0.cpp::printMenu`, `src/finance_v3_0.cpp::settingsMenu`, `config/i18n.h::I18n`)

## Non-functional requirements 
- Portability: uses standard C++17 and `std::filesystem`; includes Windows console APIs behind `#ifdef _WIN32`. (`src/finance_v3_0.cpp`)
- Data integrity: input validation and defensive parsing for save files; recompute balances from transactions on load. (`src/finance_v3_0.cpp::tryParseDate`, `src/finance_v3_0.cpp::Account::loadFromFile`)
- UX constraints: CLI-only, menu-driven, text prompts; ESC or "esc" can cancel some flows. (`src/finance_v3_0.cpp::getlineAllowEsc`)
- Localization: text keys resolved via locale files; fallback behavior if missing. (`config/i18n.h::I18n::get`)

## In-scope vs out-of-scope
- In-scope: local file persistence, single-user CLI workflows, scheduling/interest/allocation logic, language selection.
- Out-of-scope (not found in repo): multi-user accounts, cloud sync, encryption, GUI, budgeting dashboards, exporting to spreadsheets, network services.
- Unknown / Not found in repo: explicit performance targets, formal requirements spec, or documented user acceptance criteria.

# 3. Features
## Transactions
- Manual transaction entry (income/expense).
  - Inputs: date (YYYY-MM-DD or blank for today), amount (double), category (select or create), note.
  - Outputs: updates `Account::txs`, `Account::balance`, and per-category balance.
  - Limitations: strict date format; negative amounts default to "Other" if no category. (`src/finance_v3_0.cpp::main`, `src/finance_v3_0.cpp::Account::addManualTransaction`)

## Recurring schedules
- Schedule types: every N days or monthly on a day of month.
  - Inputs: schedule type, parameter (days or day-of-month), amount, category/auto-allocation, start date.
  - Outputs: creates future transactions when processed up to a target date.
  - Limitations: skips invalid parameters; guard prevents infinite loops if dates do not advance. (`src/finance_v3_0.cpp::Account::processSchedulesUpTo`)

## Allocation by percentage
- Auto-allocation for positive amounts across categories.
  - Inputs: allocation percentages per category; income amount.
  - Outputs: generated transactions for each category share.
  - Limitations: if total allocation is zero, amount is assigned to "Other". (`src/finance_v3_0.cpp::Account::allocateAmount`, `src/finance_v3_0.cpp::interactiveAllocSetup`)

## Interest
- Per-category interest rules (monthly or annual converted to monthly).
  - Inputs: category list, rate, frequency, start date.
  - Outputs: interest transactions added per month up to a date.
  - Limitations: interest only applied when category balance is positive for that month. (`src/finance_v3_0.cpp::Account::applyInterestUpTo`)

## Settings and localization
- Settings: auto-save, auto-process on startup, and language selection.
  - Inputs: menu choices; stored in save file.
  - Outputs: affects program behavior and UI language.
  - Limitations: available languages depend on locale files discovered at runtime. (`src/finance_v3_0.cpp::settingsMenu`, `config/i18n.h::I18n::availableLanguages`)

## Persistence and helpers
- Save/load to a pipe-delimited text file with escaping.
  - Inputs: filesystem path, in-memory account data.
  - Outputs: `data/save/finance_save.txt` (default) and restored state on load.
  - Limitations: no atomic save or backup found in v3.0 code. (`src/finance_v3_0.cpp::Account::saveToFile`, `src/finance_v3_0.cpp::Account::loadFromFile`)
- Helper CLI flags:
  - `--dump-loc`, `--list-locales`, `--dump-settings`, `--test-balance-load`. (`src/finance_v3_0.cpp::main`)

# 4. Usage Guide
## Build instructions (from repo)
- Recommended build command:
  - `g++ -std=c++17 src/finance_v3_0.cpp -o bin/finance_v3_0.exe` (`README.md`)
- Notes:
  - Uses `<bits/stdc++.h>` in `src/finance_v3_0.cpp`, which is GCC/MinGW-specific.
  - Unknown / Not found in repo: CMakeLists.txt, Makefile, or other build system files.

## How to run
- Windows (from repo): `bin/finance_v3_0.exe`
- The app sets its working directory to the project root based on executable location. (`src/finance_v3_0.cpp::main`)

## Typical user workflows
1) First run
   - Start program, choose setup when save file is missing.
   - Define categories and optional allocation percentages.
2) Add transactions
   - Enter a date, amount, category, and note.
   - Leave category blank for auto-allocation of income.
3) Configure schedules
   - Add recurring income/expense and optionally auto-allocate.
4) Apply interest
   - Add per-category interest rules and apply interest to today.
5) Save and exit
   - Use auto-save or manual save; exit via menu.

## Common pitfalls
- Date input must be `YYYY-MM-DD` or blank for today. (`src/finance_v3_0.cpp::tryParseDate`)
- Allocation percentages must total <= 100; "Other" gets remainder. (`src/finance_v3_0.cpp::interactiveAllocSetup`)
- `src/finance_v3_0.cpp` and `config/i18n.h` contain merge conflict markers, which will prevent compilation until resolved.
- `bits/stdc++.h` is not supported by MSVC; use MinGW/g++.

# 5. Architecture and Design
## High-level architecture
- Monolithic CLI application with a single primary source file plus a header-only i18n loader.
- Data is kept in-memory in an `Account` struct and serialized to a text file.
- Locale files provide translated UI strings; the app chooses language from settings.

## File/module responsibilities
| File/Module | Responsibility | Key Functions |
| --- | --- | --- |
| `src/finance_v3_0.cpp` | Core application logic, data model, persistence, UI, scheduling, interest | `main`, `Account::saveToFile`, `Account::loadFromFile`, `Account::processSchedulesUpTo`, `Account::applyInterestUpTo` |
| `config/i18n.h` | Header-only locale loader and lookup | `I18n::tryLoadLocalesFolder`, `I18n::get`, `I18n::availableLanguages` |
| `config/locales/*.lang` | Translation strings (EN, VI, DE, fallback) | Locale key/value data |
| `.github/workflows/ci.yml` | CI build using g++ on Linux | GitHub Actions job |
| `.vscode/tasks.json` | Local build task definition for g++ | VS Code task |

## Core data model
| Struct/Class | Fields (summary) | Purpose | Where |
| --- | --- | --- | --- |
| `Transaction` | `date`, `amount`, `category`, `note` | Represents a single income/expense entry | `src/finance_v3_0.cpp::Transaction` |
| `Schedule` | `type`, `param`, `amount`, `note`, `autoAllocate`, `nextDate`, `category` | Recurring transaction definition | `src/finance_v3_0.cpp::Schedule` |
| `InterestEntry` | `categoryNormalized`, `ratePct`, `monthly`, `startDate`, `lastAppliedDate` | Interest rule per category | `src/finance_v3_0.cpp::InterestEntry` |
| `Settings` | `autoSave`, `autoProcessOnStartup`, `language` | User preferences | `src/finance_v3_0.cpp::Settings` |
| `Account` | `balance`, `txs`, `schedules`, `allocationPct`, `categoryBalances`, `displayNames`, `interestMap`, `settings` | Primary in-memory state | `src/finance_v3_0.cpp::Account` |
| `I18n` | `locales`, `fallback`, `loadDiagnostics`, `requiredKeys` | Locale discovery and lookup | `config/i18n.h::I18n` |

## Program lifecycle flow
1) Startup: initialize ANSI/UTF-8 console, set project root, load locales. (`src/finance_v3_0.cpp::main`, `config/i18n.h::I18n`)
2) Handle helper flags (`--dump-loc`, `--list-locales`, `--dump-settings`, `--test-balance-load`) and exit if used. (`src/finance_v3_0.cpp::main`)
3) Load account from save file; if missing, run initial setup. (`src/finance_v3_0.cpp::Account::loadFromFile`, `src/finance_v3_0.cpp::runInitialSetup`)
4) Optional auto-processing of schedules and interest on startup. (`src/finance_v3_0.cpp::Account::processSchedulesUpTo`, `src/finance_v3_0.cpp::Account::applyInterestUpTo`)
5) Menu loop: accept user choice, perform action, auto-save if enabled. (`src/finance_v3_0.cpp::main`)
6) Exit: save or close according to user choice. (`src/finance_v3_0.cpp::askReturnToMenuOrSave`)

# 6. Libraries and Technologies
## Standard libraries and headers
| Header/Library | Purpose | Where used |
| --- | --- | --- |
| `<bits/stdc++.h>` | GCC/MinGW umbrella include for standard library | `src/finance_v3_0.cpp` |
| `<filesystem>` | Path handling, directory creation, file probing | `src/finance_v3_0.cpp`, `config/i18n.h` |
| `<windows.h>` | Windows console APIs (ANSI mode, code page) | `src/finance_v3_0.cpp` (guarded by `_WIN32`) |
| `<string>`, `<vector>`, `<map>`, `<unordered_map>` | Core data structures | `src/finance_v3_0.cpp`, `config/i18n.h` |
| `<fstream>`, `<sstream>`, `<iostream>` | File and console I/O | `src/finance_v3_0.cpp`, `config/i18n.h` |
| `<chrono>`, `<ctime>` | Date/time processing | `src/finance_v3_0.cpp` |
| `<algorithm>`, `<cctype>`, `<iomanip>`, `<cmath>` | Parsing, formatting, validation helpers | `src/finance_v3_0.cpp` |

## Platform-specific dependencies
- Windows console: `GetStdHandle`, `GetConsoleMode`, `SetConsoleMode`, `SetConsoleOutputCP`, `SetConsoleCP` used for ANSI and UTF-8. (`src/finance_v3_0.cpp`)
- Unix-like systems: `/proc/self/exe` used to locate executable path (fallback when available). (`src/finance_v3_0.cpp::getExecutableDir`)

## Tooling and automation
- Build: g++ with C++17. (`README.md`, `.github/workflows/ci.yml`)
- CI: GitHub Actions builds all versioned source files. (`.github/workflows/ci.yml`)
- IDE tasks: VS Code build task for active file. (`.vscode/tasks.json`)

# 7. Core Algorithms / Logic
## Scheduled transactions logic
**Where in code:** `src/finance_v3_0.cpp::Account::processSchedulesUpTo`, `src/finance_v3_0.cpp::nextMonthlyOn`, `src/finance_v3_0.cpp::addDays`

**Description:** For each schedule, validate parameters, then iterate through occurrences up to a target date. Each occurrence creates either auto-allocated income or a direct transaction. A guard count prevents infinite loops.

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

## Allocation logic
**Where in code:** `src/finance_v3_0.cpp::Account::allocateAmount`, `src/finance_v3_0.cpp::interactiveAllocSetup`

**Description:** Distribute an income amount across categories by allocation percentages. If total allocation is zero, route to "Other". Each share creates a transaction and updates balances.

```text
totalPct = sum(allocationPct)
if totalPct <= 0:
  add transaction to "Other"
else:
  for each category c:
    share = amount * (pct[c] / totalPct)
    add transaction for share in c
```

## Interest calculation logic
**Where in code:** `src/finance_v3_0.cpp::Account::applyInterestUpTo`, `src/finance_v3_0.cpp::monthsBetweenInclusive`, `src/finance_v3_0.cpp::addMonths`

**Description:** For each interest entry, compute monthly interest from the last applied date up to the target date. Each month computes category balance from a working transaction list to support compounding.

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

## Validation and parsing logic
**Where in code:** `src/finance_v3_0.cpp::tryParseDate`, `src/finance_v3_0.cpp::tryParseRate`, `src/finance_v3_0.cpp::splitEscaped`, `src/finance_v3_0.cpp::escapeForSave`

**Description:** Strict date format validation (`YYYY-MM-DD`), numeric input parsing with locale-friendly commas for interest rates, and safe parsing of pipe-delimited save files using escaping.

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

# 8. Data Storage and Data Safety
## Data files and locations
- Default save file: `data/save/finance_save.txt` (created on first save). (`src/finance_v3_0.cpp::defaultSavePath`)
- Locale files: `config/locales/*.lang` plus subfolders (loaded at runtime). (`config/i18n.h::I18n::tryLoadLocalesFolder`, `src/finance_v3_0.cpp::loadLocalesFromSubfolders`)

## Save format (pipe-delimited)
Sections are written in order:
1) `BALANCE <value>`
2) `SETTINGS` (AUTO_SAVE, AUTO_PROCESS_STARTUP, LANGUAGE)
3) `INTERESTS` (category|rate|monthly|start|lastApplied)
4) `ALLOCATIONS` (category|percent)
5) `CATEGORIES` (category|balance)
6) `SCHEDULES` (type|param|amount|auto|date|category|note)
7) `TXS` (date|amount|category|note)

**Where in code:** `src/finance_v3_0.cpp::Account::saveToFile`, `src/finance_v3_0.cpp::Account::loadFromFile`

Example snippet (format derived from code):
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

## Data safety measures
- Escaping for `|`, `\`, and newline in text fields. (`src/finance_v3_0.cpp::escapeForSave`, `src/finance_v3_0.cpp::unescapeLoaded`)
- Defensive parsing: invalid lines are skipped with warnings. (`src/finance_v3_0.cpp::Account::loadFromFile`)
- Recompute balances from transactions to avoid stale data. (`src/finance_v3_0.cpp::Account::loadFromFile`)
- Save directory creation if missing. (`src/finance_v3_0.cpp::Account::saveToFile`)

## Atomic save and recovery
- Atomic save: Unknown / Not found in repo for v3.0 (save writes directly to file).
- Backups or recovery files: Unknown / Not found in repo.
- Save file versioning: Unknown / Not found in repo.

# 9. Testing
## Testing approach
- Automated: CI builds all versioned sources with g++ on Linux. (`.github/workflows/ci.yml`)
- Manual/diagnostic: helper flags in `main` allow quick checks (`--test-balance-load`, `--dump-settings`, `--dump-loc`, `--list-locales`). (`src/finance_v3_0.cpp::main`)
- Unknown / Not found in repo: unit tests or integration test suites.

## Test cases (summary)
| ID | Steps | Expected | Actual |
| --- | --- | --- | --- |
| T01 | Run with no save file; choose setup; create categories | Categories created; no crash |  |
| T02 | Add manual income with blank category | Income auto-allocated across categories |  |
| T03 | Add schedule (EveryXDays) and process schedules | Generated transactions through today |  |
| T04 | Add interest rule and apply interest | Interest transactions added for eligible months |  |
| T05 | Run `--test-balance-load` | PASS and exit code 0 |  |
| T06 | Enter invalid date format | Re-prompt with validation message |  |

## Edge cases and failure scenarios
- Invalid schedule parameters (day <= 0 or > 31) are skipped with warnings.
- Save file lines with malformed fields are ignored; parsing continues.
- Zero or negative category balance results in zero interest for that month.
- If schedule date does not advance, processing stops to prevent infinite loops.

# 10. Conclusion and Future Work
## Completed
- Core CLI finance manager with transactions, schedules, allocation, interest, localization, and save/load.

## Known limitations
- Merge conflict markers exist in `src/finance_v3_0.cpp` and `config/i18n.h`, which will prevent compilation until resolved.
- Save operation is not atomic in v3.0; no backup/recovery mechanism found.
- No automated unit tests in the repo; only manual/diagnostic helpers.
- `bits/stdc++.h` reduces portability on non-GCC toolchains.

## Proposed improvements
- Resolve merge conflicts and verify build on Windows and Linux.
- Add atomic save and backup strategy for data safety.
- Add unit tests around parsing, scheduling, and interest logic.
- Replace `bits/stdc++.h` with explicit standard headers for portability.

# Appendix A: Project Structure
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

# Appendix B: Key Functions / Structs
| Name | Description | Where |
| --- | --- | --- |
| `Transaction` | Financial entry with date, amount, category, note | `src/finance_v3_0.cpp::Transaction` |
| `Schedule` | Recurring transaction definition | `src/finance_v3_0.cpp::Schedule` |
| `InterestEntry` | Per-category interest configuration | `src/finance_v3_0.cpp::InterestEntry` |
| `Account` | Core state and behaviors | `src/finance_v3_0.cpp::Account` |
| `Account::addManualTransaction` | Add a manual transaction and update balances | `src/finance_v3_0.cpp::Account::addManualTransaction` |
| `Account::allocateAmount` | Allocate income across categories | `src/finance_v3_0.cpp::Account::allocateAmount` |
| `Account::processSchedulesUpTo` | Generate scheduled transactions up to a date | `src/finance_v3_0.cpp::Account::processSchedulesUpTo` |
| `Account::applyInterestUpTo` | Apply interest per category up to a date | `src/finance_v3_0.cpp::Account::applyInterestUpTo` |
| `Account::saveToFile` | Serialize account state to disk | `src/finance_v3_0.cpp::Account::saveToFile` |
| `Account::loadFromFile` | Load and validate account state | `src/finance_v3_0.cpp::Account::loadFromFile` |
| `tryParseDate` | Strict date parsing (YYYY-MM-DD) | `src/finance_v3_0.cpp::tryParseDate` |
| `tryParseRate` | Interest rate parsing with comma/percent support | `src/finance_v3_0.cpp::tryParseRate` |
| `I18n` | Locale loading and translation lookup | `config/i18n.h::I18n` |
| `main` | Program entry point and menu loop | `src/finance_v3_0.cpp::main` |

# Appendix C: Test Cases (Expanded)
## T01 - First run setup
- Steps: delete or rename save file, run app, choose setup, enter categories, skip allocation setup.
- Expected: categories created; defaults applied for "Other"; app returns to menu.
- Actual: 

## T02 - Manual income auto-allocation
- Steps: create categories with allocations, add income transaction with blank category.
- Expected: transactions created for each category share; balances updated.
- Actual: 

## T03 - Recurring schedule processing
- Steps: add EveryXDays schedule starting today; process schedules up to today.
- Expected: at least one scheduled transaction added; nextDate advanced.
- Actual: 

## T04 - Interest application
- Steps: set monthly interest for a category with positive balance; apply interest to today.
- Expected: interest transaction added; balance increases.
- Actual: 

## T05 - Save and reload
- Steps: add transactions, save, exit, relaunch and load.
- Expected: transactions and balances restored; recomputed balance matches transactions.
- Actual: 

## T06 - Invalid date input
- Steps: enter an invalid date (e.g., 2024-13-40).
- Expected: validation error; re-prompt for date.
- Actual: 

# Appendix D: Screenshots / Images
- No images found in repo.
- Placeholders to add:
  - Main menu screen
  - Add transaction flow
  - Schedule list/summary
  - Interest setup screen
