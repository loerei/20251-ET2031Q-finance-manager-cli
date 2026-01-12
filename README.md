# 20251-ET2031Q-Finance Manager

A multilingual command-line program for logging income and expenses, managing recurring items, organizing categories, and tracking your balance.    

## Features
- Manual income/expense transactions with notes
- Recurring schedules (every X days or monthly on a day)
- Category allocations with per-category balances
- Per-category interest rules (monthly or annual)
- Settings for auto-save, auto-process on startup, and language
- Atomic save format with escaping and recovery safeguards
- Portable path resolution (runs from any working directory)
- i18n loader with locale file discovery in subfolders

## Project layout
- `src/` - main sources by version (latest: `finance_v3_0.cpp`) and test sources
- `bin/` - prebuilt executables (latest: `finance_v3_0.exe`)
- `config/` - `i18n.h` plus locale files in `config/locales/`
- `data/save/` - persistent save data (`finance_save.txt`)
- `docs/` - launching-focused documentation
- `Reference/` - test artifacts and helper scripts (including atomic save runner)
- `output/` - optional debug output
- `tests/` - reserved for additional tests (currently empty)

## Project docs
- [Work assignment](docs/WORKASSIGNMENT.md)
- [Report (EN)](docs/REPORT.md)
- [Report (VI)](docs/REPORT_VI.md)

## Build
From the project root:
```bash
g++ -std=c++17 src/finance_v3_0.cpp -o bin/finance_v3_0.exe
```

## Run
```bash
bin/finance_v3_0.exe
```

The app resolves its project root at runtime, so it can be launched from any working directory.

## Helper flags
- `--dump-loc <CODE>`: print a small set of keys from a locale
- `--list-locales`: list loaded locale files and available language codes
- `--dump-settings`: print current settings from the save file
- `--test-balance-load`: regression check for balance recomputation

## Localization
Locale files live in `config/locales/` and can be split into base and `_extra` files. Version 3.0 also scans nested folders under `config/locales/` for additional locale packs. Use `--list-locales` to confirm what loaded.

## Save data
Default save file:
```
data/save/finance_save.txt
```
The file is created automatically on the first run.

## Version history
- See `CHANGELOG.md` for versions 1.0 through 3.0.
- Latest source and binary in this repo: `src/finance_v3_0.cpp` and `bin/finance_v3_0.exe`.

## Flowchart
- Miro board: [Miro Flowchart](https://miro.com/app/board/uXjVGd4fsqQ=/?share_link_id=714558277659)
