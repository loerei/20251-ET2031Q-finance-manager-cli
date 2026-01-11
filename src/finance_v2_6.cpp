// Finance Manager v2.6 - Personal Finance Management System
// 
// CLI-based personal finance manager with support for:
//   - Manual transactions (income/expenses) with categories and notes
//   - Recurring schedules with date-based processing
//   - Category allocations and balances
//   - Savings interest calculations
//   - Persistent save/load of account data
// 
// Architecture Overview:
//   1. Core data structures: transactions, schedules, account state
//   2. Utilities: date parsing/formatting and string helpers
//   3. Persistence: save/load serialization and parsing
//   4. User interface: menu-driven CLI workflows
//   5. Main loop: initialization, menu dispatch, graceful exit
// 
// Build: g++ -std=c++17 finance_v2_6.cpp -o finance_v2_6
// Usage: ./finance_v2_6 (interactive mode)

#ifdef _WIN32
#include <windows.h>
#endif

#include <bits/stdc++.h>
#include <filesystem>
using namespace std;
using chrono_tp = chrono::system_clock::time_point;

////////////////////////////////////////////////////////////////////////////////
// SECTION 1: CORE DATA STRUCTURES
// Defines Transaction, Schedule, InterestEntry, Settings, and Account structs
////////////////////////////////////////////////////////////////////////////////

// Transaction: represents a single financial entry (income or expense)
struct Transaction {
    chrono_tp date;
    double amount; // positive = income, negative = expense
    string category; // display name
    string note;
};

// Schedule types: recurring transactions can repeat every X days or monthly on a specific day
enum class ScheduleType { EveryXDays, MonthlyDay };

// Schedule: represents a recurring transaction (e.g., salary every month, rent on 1st, etc.)
struct Schedule {
    ScheduleType type;
    int param; // days interval or day-of-month
    double amount;
    string note;
    bool autoAllocate;
    chrono_tp nextDate;
    string category; // display name (may be empty => use Other or auto-alloc when appropriate)
};

// Interest entry per-category: stores interest rate rules for earning interest on balances
struct InterestEntry {
    string categoryNormalized; // normalized key for lookup
    double ratePct; // stored as percent (e.g., 0.5 means 0.5%)
    bool monthly; // true if monthly rate, false if annual rate
    chrono_tp startDate; // when the rate starts applying
    chrono_tp lastAppliedDate; // last date interest was applied through (initially = startDate)
};

// Settings: user preferences for behavior and language
struct Settings {
    bool autoSave = false;
    bool autoProcessOnStartup = false;
    string language = "EN"; // "EN", "VI", "DE", etc.
};

// Forward declaration for translation helper (defined later)
static inline string tr(const Settings &s, const string &id);

////////////////////////////////////////////////////////////////////////////////
// SECTION 2: FILESYSTEM & PATH HELPERS
// Manages executable location detection and save file paths
////////////////////////////////////////////////////////////////////////////////

// Get the directory containing the executable (works regardless of working directory)
// Uses /proc/self/exe on Unix or __FILE__ fallback on Windows/macOS
static inline std::string getExecutableDir() {
    std::error_code ec;
    std::filesystem::path exePath = std::filesystem::canonical("/proc/self/exe", ec);
    if (ec) {
        // Fallback for Windows or when /proc/self/exe doesn't work:
        // Use the source file directory (parent of src/)
        exePath = std::filesystem::canonical(std::filesystem::path(__FILE__).parent_path().parent_path(), ec);
    }
    return exePath.string();
}

// Save file path: ../data/save/finance_save.txt (relative to source file location)
static inline const std::string SAVE_FILENAME = (std::filesystem::path(__FILE__).parent_path() / ".." / "data" / "save" / "finance_save.txt").string();

////////////////////////////////////////////////////////////////////////////////
// SECTION 3: TIME & DATE HELPERS
// Provides date parsing, formatting, and arithmetic operations
////////////////////////////////////////////////////////////////////////////////

// -------------------- safe localtime --------------------
// Cross-platform wrapper for localtime() - handles Windows, macOS, and Linux differences
static inline tm safeLocaltime(time_t tt) {
    tm result{};
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
    localtime_s(&result, &tt);
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    localtime_r(&tt, &result);
#else
    tm *tmp = localtime(&tt);
    if (tmp) result = *tmp;
#endif
    return result;
}

// -------------------- Date parsing & formatting --------------------
// tryParseDate: Parse YYYY-MM-DD string format into chrono_tp
// Returns false if parsing fails or date is invalid
static inline bool tryParseDate(const string &s, chrono_tp &out) {
    if (s.empty()) return false;
    tm t = {};
    istringstream ss(s);
    ss >> get_time(&t, "%Y-%m-%d");
    if (ss.fail()) return false;
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
    t.tm_isdst = -1;
    time_t tt = mktime(&t);
    if (tt == -1) return false;
    out = std::chrono::system_clock::from_time_t(tt);
    return true;
}

// toDateString: Convert chrono_tp to YYYY-MM-DD string format
static inline string toDateString(const chrono_tp &tp) {
    time_t tt = chrono::system_clock::to_time_t(tp);
    tm t = safeLocaltime(tt);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
    return string(buf);
}

// today: Get current date at midnight (00:00:00)
static inline chrono_tp today() {
    auto now = chrono::system_clock::now();
    time_t tt = chrono::system_clock::to_time_t(now);
    tm t = safeLocaltime(tt);
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0; t.tm_isdst = -1;
    return chrono::system_clock::from_time_t(mktime(&t));
}

// -------------------- Date arithmetic --------------------
// daysInMonth: Return number of days in given month (handles leap years)
static inline int daysInMonth(int year, int month) {
    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2) {
        bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        return 28 + (leap ? 1 : 0);
    }
    return mdays[month - 1];
}

// addDays: Add N days to a date
static inline chrono_tp addDays(const chrono_tp &tp, int days) {
    time_t tt = chrono::system_clock::to_time_t(tp);
    tm t = safeLocaltime(tt);
    t.tm_mday += days;
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0; t.tm_isdst = -1;
    time_t newt = mktime(&t);
    return chrono::system_clock::from_time_t(newt);
}

// addMonths: Add N months to a date (handles month/year boundaries and day-of-month capping)
// Example: 2024-01-31 + 1 month = 2024-02-29 (capped to last day of Feb)
static inline chrono_tp addMonths(const chrono_tp &tp, int months) {
    time_t tt = chrono::system_clock::to_time_t(tp);
    tm t = safeLocaltime(tt);
    int year = t.tm_year + 1900;
    int month = t.tm_mon + 1;
    int day = t.tm_mday;

    int totalMonths = month - 1 + months;
    int newYear = year + totalMonths / 12;
    int newMonth = (totalMonths % 12) + 1;
    if (newMonth <= 0) { newMonth += 12; newYear -= 1; }

    int last = daysInMonth(newYear, newMonth);
    int useDay = min(day, last);

    tm t2 = {};
    t2.tm_year = newYear - 1900;
    t2.tm_mon = newMonth - 1;
    t2.tm_mday = useDay;
    t2.tm_hour = 0; t2.tm_min = 0; t2.tm_sec = 0; t2.tm_isdst = -1;
    time_t newt = mktime(&t2);
    return chrono::system_clock::from_time_t(newt);
}

// nextMonthlyOn: Get next occurrence of a specific day-of-month from a given date
// Example: from 2024-01-15, day=1 returns 2024-02-01
static inline chrono_tp nextMonthlyOn(const chrono_tp &from, int day) {
    time_t tt = chrono::system_clock::to_time_t(from);
    tm t = safeLocaltime(tt);
    int curDay = t.tm_mday;
    int year = t.tm_year + 1900;
    int month = t.tm_mon + 1;

    if (curDay < day) {
        int last = daysInMonth(year, month);
        int useDay = min(day, last);
        t.tm_mday = useDay;
        t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0; t.tm_isdst = -1;
        return chrono::system_clock::from_time_t(mktime(&t));
    } else {
        month += 1;
        if (month > 12) { month = 1; year += 1; }
        int last = daysInMonth(year, month);
        int useDay = min(day, last);
        tm t2 = {};
        t2.tm_year = year - 1900;
        t2.tm_mon = month - 1;
        t2.tm_mday = useDay;
        t2.tm_hour = 0; t2.tm_min = 0; t2.tm_sec = 0; t2.tm_isdst = -1;
        return chrono::system_clock::from_time_t(mktime(&t2));
    }
}

// monthsBetweenInclusive: Count months from start date to end date (inclusive)
// Used by interest calculation to determine how many months of interest to apply
// Example: 2024-01-15 to 2024-03-20 = 3 months (Jan, Feb, Mar)
static inline int monthsBetweenInclusive(const chrono_tp &start, const chrono_tp &end) {
    time_t stt = chrono::system_clock::to_time_t(start);
    time_t ett = chrono::system_clock::to_time_t(end);
    if (ett < stt) return 0;
    tm s = safeLocaltime(stt);
    tm e = safeLocaltime(ett);
    int sy = s.tm_year + 1900, sm = s.tm_mon + 1, sd = s.tm_mday;
    int ey = e.tm_year + 1900, em = e.tm_mon + 1, ed = e.tm_mday;
    int months = (ey - sy) * 12 + (em - sm);
    if (ed >= sd) months += 1;
    return max(0, months);
}

// -------------------- Escaping helpers --------------------

// ============================================================
// SECTION 4: ESCAPING, PARSING & STRING UTILITIES
// ============================================================
// Functions for safe storage of data containing delimiters,
// category name normalization, and parsing user input.

// -------------------- Escape/Unescape for Pipe-Delimited Format --------------------
// escapeForSave: Escape special characters for pipe-delimited file format
// Escapes: '\' -> '\\', '|' -> '\|', '\n' -> '\n'
// This allows safe storage of text containing delimiters and newlines
static inline string escapeForSave(const string &s) {
    string out;
    out.reserve(s.size()*2);
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '|') out += "\\|";
        else if (c == '\n') out += "\\n";
        else out.push_back(c);
    }
    return out;
}

// unescapeLoaded: Reverse escapeForSave - restore original string from escaped format
// Handles: '\\n' -> '\n', '\\|' -> '|', '\\\\' -> '\'
static inline string unescapeLoaded(const string &s) {
    string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            char n = s[i+1];
            if (n == 'n') { out.push_back('\n'); ++i; }
            else { out.push_back(n); ++i; }
        } else out.push_back(c);
    }
    return out;
}

// splitEscaped: Split pipe-delimited string while respecting escape sequences
// Returns vector of unescaped parts
static inline vector<string> splitEscaped(const string &s) {
    vector<string> parts;
    string cur;
    cur.reserve(s.size());
    bool esc = false;
    for (char c : s) {
        if (esc) {
            cur.push_back(c);
            esc = false;
        } else if (c == '\\') {
            esc = true;
        } else if (c == '|') {
            parts.push_back(cur);
            cur.clear();
        } else cur.push_back(c);
    }
    parts.push_back(cur);
    for (auto &p : parts) p = unescapeLoaded(p);
    return parts;
}

// -------------------- Category Name Handling --------------------
// normalizeKey: Convert category display name to normalized lowercase key for storage
// Trims whitespace, converts to lowercase, returns "other" for empty strings
// Used to maintain category case-insensitivity internally
static inline string normalizeKey(const string &s) {
    size_t i = 0, j = s.size();
    while (i < s.size() && isspace((unsigned char)s[i])) ++i;
    while (j > i && isspace((unsigned char)s[j-1])) --j;
    string out;
    out.reserve(j - i);
    for (size_t k = i; k < j; ++k) out.push_back((char)tolower((unsigned char)s[k]));
    if (out.empty()) return string("other");
    return out;
}

// sanitizeDisplayName: Clean category name for user display
// Removes non-alphanumeric characters (keeps letters, digits, spaces)
// Collapses multiple spaces, trims edges, returns "Category" for empty strings
// This prevents saving invalid characters to files and ensures safe display
static inline string sanitizeDisplayName(const string &s) {
    string tmp;
    tmp.reserve(s.size());
    for (char c : s) {
        if (isalnum((unsigned char)c) || isspace((unsigned char)c)) tmp.push_back(c);
        // else skip character
    }
    // collapse consecutive spaces
    string out;
    bool lastSpace = false;
    for (char c : tmp) {
        if (isspace((unsigned char)c)) {
            if (!lastSpace) { out.push_back(' '); lastSpace = true; }
        } else {
            out.push_back(c);
            lastSpace = false;
        }
    }
    // trim leading and trailing whitespace
    while (!out.empty() && isspace((unsigned char)out.front())) out.erase(out.begin());
    while (!out.empty() && isspace((unsigned char)out.back())) out.pop_back();
    if (out.empty()) return string("Category");
    return out;
}

// -------------------- Numeric Input Parsing --------------------
// tryParseRate: Parse user-entered interest rate string
// Accepts formats: "0.5", "0,5" (locale-aware), "0.5%", "0,5%"
// Returns rate as decimal percent (e.g., "0.5%" -> 0.5 stored as 0.5)
// Returns false if parsing fails
static inline bool tryParseRate(const string &sraw, double &outPct) {
    string s = sraw;
    // remove all spaces
    s.erase(remove_if(s.begin(), s.end(), [](char c){ return isspace((unsigned char)c); }), s.end());
    // replace comma with dot for locale compatibility
    for (char &c : s) if (c == ',') c = '.';
    // remove trailing % if present
    bool hadPercent = false;
    if (!s.empty() && s.back() == '%') { hadPercent = true; s.pop_back(); }
    if (s.empty()) return false;
    try {
        size_t pos = 0;
        double v = stod(s, &pos);
        if (pos != s.size()) return false;
        outPct = v;
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================
// SECTION 5: ACCOUNT MANAGEMENT
// ============================================================
// Main account struct managing all financial data:
// - Overall balance and transaction history
// - Category organization with allocation percentages
// - Scheduled transaction processing and interest application
// - File persistence (loading/saving)

struct Account {
    // Core financial data
    double balance = 0.0;                    // Total account balance
    vector<Transaction> txs;                 // All transactions (manual + scheduled + interest)
    vector<Schedule> schedules;              // Scheduled transactions (recurring income/expenses)

    // Category management (all use normalized lowercase keys for case-insensitivity)
    map<string, double> allocationPct;       // normalized -> allocation percent (for auto-allocation)
    map<string, double> categoryBalances;    // normalized -> amount (sum of transactions in category)
    map<string, string> displayNames;        // normalized -> display name (user-facing category name)

    // Interest tracking
    map<string, InterestEntry> interestMap;  // normalized -> interest entry (rate + application history)

    // User settings
    Settings settings;

    // Constructor: Initialize with default categories and settings
    Account() {
        // Create default category allocations
        vector<pair<string,double>> defaults = {
            {"Emergency", 20.0}, {"Entertainment", 10.0}, {"Saving", 20.0}, {"Other", 50.0}
        };
        for (auto &p : defaults) {
            string nk = normalizeKey(p.first);
            allocationPct[nk] = p.second;
            displayNames[nk] = p.first;
            categoryBalances[nk] = 0.0;
        }
        // settings defaults already set by Settings ctor
    }

    // ---- Category management helpers ----
    // Ensure a category exists in the internal maps with zero balance
    // Does nothing if category already exists
    void ensureCategoryExists(const string &displayRaw) {
        string display = sanitizeDisplayName(displayRaw);
        string nk = normalizeKey(display);
        if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = display;
        if (categoryBalances.find(nk) == categoryBalances.end()) categoryBalances[nk] = 0.0;
        if (allocationPct.find(nk) == allocationPct.end()) allocationPct[nk] = 0.0;
    }

    // Add a manual transaction to a specific category
    // Handles: transaction history, category creation, balance updates
    // Supports positive (income) and negative (expense) amounts
    void addManualTransaction(const chrono_tp &date, double amount, const string &categoryRaw, const string &note) {
        string catDisplay = categoryRaw.empty() ? "Other" : sanitizeDisplayName(categoryRaw);
        string nk = normalizeKey(catDisplay);
        if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = catDisplay;
        Transaction t{date, amount, displayNames[nk], note};
        txs.push_back(t);
        balance += amount;
        if (categoryBalances.find(nk) == categoryBalances.end())
            categoryBalances[nk] = 0.0;
        categoryBalances[nk] += amount;
    }

    // ---- Allocation management ----
    // Set category allocation percentages from user-provided map
    // Clears existing allocations and rebuilds from input
    // Ensures all categories exist with zero balance
    void setAllocation(const map<string,double> &newAlloc) {
        allocationPct.clear();
        for (auto &p : newAlloc) {
            string dk = sanitizeDisplayName(p.first);
            string nk = normalizeKey(dk);
            allocationPct[nk] = p.second;
            if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = dk;
            if (categoryBalances.find(nk) == categoryBalances.end()) categoryBalances[nk] = 0.0;
        }
    }

    // Allocate an amount across categories based on allocation percentages
    // Creates transactions for each category with their proportional share
    // Falls back to "Other" if no allocations are defined
    // Used for: scheduled income allocation, manual allocation operations
    void allocateAmount(const chrono_tp &date, double amount, const string &note) {
        double totalPct = 0;
        for (auto &p : allocationPct) totalPct += p.second;
        if (totalPct <= 0.000001) {
            string nk = normalizeKey("Other");
            if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = "Other";
            categoryBalances[nk] += amount;
            txs.push_back({date, amount, displayNames[nk], note + " (auto alloc fallback)"});
            balance += amount;
            return;
        }
        for (auto &p : allocationPct) {
            double share = amount * (p.second / totalPct);
            categoryBalances[p.first] += share;
            string catDisplay = displayNames[p.first].empty() ? p.first : displayNames[p.first];
            txs.push_back({date, share, catDisplay, note + " (auto alloc)"});
            balance += share;
        }
    }

    // ---- Schedule management ----
    // Add a schedule (recurring transaction) to the account
    void addSchedule(const Schedule &s) {
        schedules.push_back(s);
    }

    // Process all scheduled transactions up to a given date (usually today)
    // Handles two schedule types:
    //   - EveryXDays: Repeats every N days
    //   - MonthlyDay: Repeats on a specific day-of-month
    // For each schedule occurrence:
    //   - If autoAllocate=true and amount>0: Allocate across categories
    //   - Otherwise: Add as manual transaction to specified category
    // Guard limit of 10,000 iterations prevents infinite loops for broken schedules
    void processSchedulesUpTo(const chrono_tp &upTo) {
        for (auto &s : schedules) {
            if (s.type == ScheduleType::EveryXDays && s.param <= 0) {
                cerr << "Skipping schedule with non-positive interval (EveryXDays param=" << s.param << ")\n";
                continue;
            }
            if (s.type == ScheduleType::MonthlyDay && (s.param < 1 || s.param > 31)) {
                cerr << "Skipping schedule with invalid day-of-month (param=" << s.param << ")\n";
                continue;
            }
            int guard = 0;
            while (s.nextDate <= upTo && guard < 10000) {
                // If autoAllocate && amount > 0 => allocate
                if (s.autoAllocate && s.amount > 0.0) {
                    allocateAmount(s.nextDate, s.amount, "Scheduled: " + s.note);
                } else {
                    // Use schedule.category if given, else default to "Other"
                    string cat = s.category.empty() ? string("Other") : s.category;
                    addManualTransaction(s.nextDate, s.amount, cat, "Scheduled: " + s.note);
                }

                if (s.type == ScheduleType::EveryXDays) s.nextDate = addDays(s.nextDate, s.param);
                else s.nextDate = nextMonthlyOn(s.nextDate, s.param);
                ++guard;
            }
            if (guard >= 10000) {
                cerr << "Warning: schedule processing hit guard limit for a schedule. Skipping further iterations for safety.\n";
            }
        }
    }

    // ---- Interest calculation & application ----
    // Apply interest for all configured rates up to a given date
    // Uses month-by-month simulation to properly handle:
    //   - Interest compounding (new interest is applied to future month calculations)
    //   - Scheduled transactions (included in balance when calculating interest)
    //   - Category-specific calculations (separate for each interest entry)
    //
    // For each interest entry:
    //   1. Calculate number of months from lastAppliedDate to upTo (inclusive)
    //   2. For each month: Calculate category balance at month end, apply interest
    //   3. Create interest transaction and add to transaction history
    //   4. Interest amount is added to overall and category balances
    //   5. Move lastAppliedDate forward to mark progress
    //
    // Important: If balance <= 0 in a month, no interest is applied (but date still advances)
    void applyInterestUpTo(const chrono_tp &upTo) {
        if (interestMap.empty()) return;

        // We'll snapshot existing transactions and then for each interest entry, simulate month-by-month.
        // To avoid cross-interference between categories during simulation, we'll make a working tx list that is global:
        vector<Transaction> workingTxs = txs; // includes all existing transactions

        for (auto &kv : interestMap) {
            InterestEntry &ie = kv.second;
            // If startDate > upTo, skip
            if (ie.startDate > upTo) continue;

            // Determine months to apply from ie.lastAppliedDate (or startDate) up to upTo
            chrono_tp fromDate = ie.lastAppliedDate;
            if (fromDate < ie.startDate) fromDate = ie.startDate;
            int months = monthsBetweenInclusive(fromDate, upTo);
            if (months <= 0) continue;

            // monthly rate to use for compounding:
            double monthlyRate = 0.0;
            if (ie.monthly) monthlyRate = ie.ratePct / 100.0;
            else monthlyRate = (ie.ratePct / 100.0) / 12.0;

            // starting point for iterative months: we'll start at fromDate advanced by 0 .. months-1
            chrono_tp monthBase = fromDate;
            for (int m = 0; m < months; ++m) {
                // compute applyDate as addMonths(fromDate, m)
                chrono_tp applyDate = addMonths(fromDate, m);

                // compute balance for this category in workingTxs up to applyDate (inclusive)
                double bal = 0.0;
                string nk = ie.categoryNormalized;
                for (auto &t : workingTxs) {
                    string tnk = normalizeKey(t.category);
                    if (tnk != nk) continue;
                    if (t.date <= applyDate) bal += t.amount;
                }
                if (bal <= 0.0) {
                    // even if zero, we should move lastAppliedDate forward
                    // create lastAppliedDate advancement below
                } else {
                    double interest = bal * monthlyRate;
                    if (interest != 0.0) {
                        // Create transaction at applyDate
                        string display = displayNames.count(nk) ? displayNames[nk] : nk;
                        Transaction itx; itx.date = applyDate; itx.amount = interest; itx.category = display; itx.note = string("Interest (") + (ie.monthly ? "monthly" : "annual/converted to monthly") + ")";
                        // append to both account txs and workingTxs so subsequent months compound
                        txs.push_back(itx);
                        workingTxs.push_back(itx);
                        // apply to balances and overall balance
                        categoryBalances[nk] += interest;
                        balance += interest;
                    }
                }
            }

            // advance lastAppliedDate by 'months' months
            ie.lastAppliedDate = addMonths(fromDate, months);
            // ensure lastAppliedDate is not in future beyond upTo
            if (ie.lastAppliedDate > upTo) ie.lastAppliedDate = upTo;
        }
    }

    // ---- Display & reporting ----
    // Print detailed summary of account state including all balances, allocations, and recent transactions
    void printSummary() {
        cout << "==== Account Summary ====\n";
        cout << "\n\nTotal balance: " << fixed << setprecision(2) << balance << "\n";
        cout << "Category balances:\n";
        for (auto &p : categoryBalances) {
            string display = displayNames[p.first].empty() ? p.first : displayNames[p.first];
            cout << "  - " << display << ": " << fixed << setprecision(2) << p.second << "\n";
        }
        cout << "\n\nAllocations (%):\n";
        for (auto &p : allocationPct) {
            string display = displayNames[p.first].empty() ? p.first : displayNames[p.first];
            cout << "  - " << display << ": " << p.second << "%\n";
        }
        cout << "\n\nInterest entries:\n";
        for (auto &kv : interestMap) {
            const InterestEntry &ie = kv.second;
            string display = displayNames.count(ie.categoryNormalized) ? displayNames.at(ie.categoryNormalized) : ie.categoryNormalized;
            cout << "  - " << display << ": " << ie.ratePct << (ie.monthly ? "% monthly" : "% annual (converted monthly)") 
                 << ", start=" << toDateString(ie.startDate) << ", lastApplied=" << toDateString(ie.lastAppliedDate) << "\n";
        }
        cout << "\n\nScheduled transactions: " << schedules.size() << "\n";
        for (size_t i = 0; i < schedules.size(); ++i) {
            auto &s = schedules[i];
            cout << "  [" << i << "] amount=" << s.amount << " next=" << toDateString(s.nextDate)
                 << " type=" << (s.type==ScheduleType::EveryXDays? "EveryXDays":"MonthlyDay")
                 << " param=" << s.param << " autoAlloc=" << (s.autoAllocate? "yes":"no")
                 << " category=" << (s.category.empty() ? string("<<auto/Other>>") : s.category)
                 << " note=" << s.note << "\n";
        }
        cout << "\n\nRecent transactions (last 10):\n";
        int start = max(0, (int)txs.size()-10);
        for (int i = (int)txs.size()-1; i >= start; --i)
            cout << toDateString(txs[i].date) << " | " << setw(10) << txs[i].amount
                 << " | " << txs[i].category << " | " << txs[i].note << "\n";
        cout << "=========================\n";
    }

    // ---- Persistence (save/load) ----
    // Save entire account state to file in pipe-delimited format
    // Format includes: balance, settings, interest rates, allocations, categories, schedules, transactions
    // All text fields are escaped to safely handle special characters
    // Creates parent directories if needed
    void saveToFile(const string &filename = SAVE_FILENAME) {
        try {
            std::filesystem::path ppath(filename);
            if (!ppath.parent_path().empty()) std::filesystem::create_directories(ppath.parent_path());
        } catch (...) { /* ignore directory creation errors */ }
        ofstream ofs(filename);
        if (!ofs) { cerr << "Cannot open file to save: " << filename << "\n"; return; }
        ofs << fixed << setprecision(10);
        ofs << "BALANCE " << balance << "\n";
        // SETTINGS
        ofs << "SETTINGS\n";
        ofs << "AUTO_SAVE|" << (settings.autoSave ? "1" : "0") << "\n";
        ofs << "AUTO_PROCESS_STARTUP|" << (settings.autoProcessOnStartup ? "1" : "0") << "\n";
        ofs << "LANGUAGE|" << settings.language << "\n";

        ofs << "INTERESTS\n";
        // Save: category|rate|monthly|start|lastApplied
        for (auto &kv : interestMap) {
            auto &ie = kv.second;
            string display = displayNames.count(ie.categoryNormalized) ? displayNames[ie.categoryNormalized] : ie.categoryNormalized;
            ofs << escapeForSave(display) << "|" << ie.ratePct << "|" << (ie.monthly ? "1" : "0")
                << "|" << escapeForSave(toDateString(ie.startDate)) << "|" << escapeForSave(toDateString(ie.lastAppliedDate)) << "\n";
        }
        ofs << "ALLOCATIONS\n";
        for (auto &p : allocationPct) {
            string display = displayNames[p.first].empty() ? p.first : displayNames[p.first];
            ofs << escapeForSave(display) << "|" << p.second << "\n";
        }
        ofs << "CATEGORIES\n";
        for (auto &p : categoryBalances) {
            string display = displayNames[p.first].empty() ? p.first : displayNames[p.first];
            ofs << escapeForSave(display) << "|" << p.second << "\n";
        }
        ofs << "SCHEDULES\n";
        // Save: type|param|amount|auto|date|category|note
        for (auto &s : schedules) {
            ofs << (s.type==ScheduleType::EveryXDays? "E":"M") << "|"
                << s.param << "|" << s.amount << "|"
                << (s.autoAllocate ? "1" : "0") << "|" << escapeForSave(toDateString(s.nextDate)) << "|"
                << escapeForSave(s.category) << "|" << escapeForSave(s.note) << "\n";
        }
        ofs << "TXS\n";
        for (auto &t : txs) {
            ofs << escapeForSave(toDateString(t.date)) << "|" << t.amount << "|" << escapeForSave(t.category) << "|" << escapeForSave(t.note) << "\n";
        }
        ofs.close();
        // UI message: show in user's language
        cout << tr(settings, "saved_to") << filename << "\n";
    }

    // Load account state from file (inverse of saveToFile)
    // Returns true if successful, false if file missing/unreadable
    // Falls back to working directory if new location not found (legacy support)
    // Silently initializes defaults for missing settings
    // Returns false without raising exceptions - caller decides behavior
    bool loadFromFile(const string &filename = SAVE_FILENAME) {
        ifstream ifs(filename);
        if (!ifs) {
            // Try legacy location (working directory) if the new location does not exist
            try {
                std::filesystem::path p(filename);
                auto base = p.filename().string();
                if (base != filename) ifs.open(base);
            } catch (...) { /* ignore */ }
            if (!ifs) {
                // do not treat as fatal here, caller will decide to set up or retry
                return false;
            }
        }
        string line;
        enum Section { None, SettingsSec, InterestSec, Alloc, Cats, Scheds, Txs } sec = None;
        allocationPct.clear(); categoryBalances.clear(); schedules.clear(); txs.clear(); displayNames.clear(); interestMap.clear();

        double savedBalance = 0.0;
        bool hadSavedBalance = false;

        // default settings if not present in file
        settings.autoSave = false;
        settings.autoProcessOnStartup = false;
        settings.language = "EN";

        while (getline(ifs, line)) {
            if (line == "SETTINGS") { sec = SettingsSec; continue; }
            if (line == "INTERESTS") { sec = InterestSec; continue; }
            if (line == "ALLOCATIONS") { sec = Alloc; continue; }
            if (line == "CATEGORIES") { sec = Cats; continue; }
            if (line == "SCHEDULES") { sec = Scheds; continue; }
            if (line == "TXS") { sec = Txs; continue; }
            if (line.rfind("BALANCE ", 0) == 0) {
                try { savedBalance = stod(line.substr(8)); hadSavedBalance = true; } catch (...) { cerr << "Warning: invalid BALANCE value.\n"; }
            } else {
                if (sec == SettingsSec) {
                    auto parts = splitEscaped(line);
                    if (parts.size() >= 1) {
                        // Expect key|value format
                        auto kv = splitEscaped(line);
                        // But splitEscaped above will parse '|' separators, so we can split by simple parsing
                        vector<string> kvp;
                        {
                            string cur; bool esc=false;
                            for (char c : line) {
                                if (esc) { cur.push_back(c); esc=false; }
                                else if (c=='\\') esc=true;
                                else if (c=='|') { kvp.push_back(cur); cur.clear(); }
                                else cur.push_back(c);
                            }
                            kvp.push_back(cur);
                        }
                        if (kvp.size() >= 2) {
                            string key = kvp[0];
                            string val = kvp[1];
                            if (key == "AUTO_SAVE") settings.autoSave = (val == "1" || val == "true");
                            else if (key == "AUTO_PROCESS_STARTUP") settings.autoProcessOnStartup = (val == "1" || val == "true");
                            else if (key == "LANGUAGE") settings.language = val;
                        }
                    }
                } else if (sec == InterestSec) {
                    auto parts = splitEscaped(line);
                    // category|rate|monthly|start|lastApplied
                    if (parts.size() >= 5) {
                        string display = parts[0];
                        double rate = 0.0;
                        bool monthly = false;
                        chrono_tp startd, lastd;
                        try { rate = stod(parts[1]); } catch (...) { cerr << "Warning: invalid interest rate for " << display << "\n"; continue; }
                        monthly = (parts[2] == "1" || parts[2] == "true");
                        if (!tryParseDate(parts[3], startd)) { cerr << "Warning: invalid interest start date '" << parts[3] << "'. Using today.\n"; startd = today(); }
                        if (!tryParseDate(parts[4], lastd)) { lastd = startd; }
                        string nk = normalizeKey(display);
                        InterestEntry ie;
                        ie.categoryNormalized = nk;
                        ie.ratePct = rate;
                        ie.monthly = monthly;
                        ie.startDate = startd;
                        ie.lastAppliedDate = lastd;
                        interestMap[nk] = ie;
                        if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = display;
                    } else cerr << "Warning: invalid interest line: " << line << "\n";
                } else if (sec == Alloc) {
                    auto parts = splitEscaped(line);
                    if (parts.size() >= 2) {
                        string display = parts[0];
                        double v = 0.0;
                        try { v = stod(parts[1]); } catch (...) { cerr << "Warning: invalid allocation for " << display << "\n"; continue; }
                        string nk = normalizeKey(display);
                        allocationPct[nk] = v;
                        if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = display;
                    } else cerr << "Warning: invalid allocation line: " << line << "\n";
                } else if (sec == Cats) {
                    auto parts = splitEscaped(line);
                    if (parts.size() >= 2) {
                        string display = parts[0];
                        double v = 0.0;
                        try { v = stod(parts[1]); } catch (...) { cerr << "Warning: invalid category balance for " << display << "\n"; continue; }
                        string nk = normalizeKey(display);
                        categoryBalances[nk] = v;
                        if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = display;
                    } else cerr << "Warning: invalid category line: " << line << "\n";
                } else if (sec == Scheds) {
                    auto parts = splitEscaped(line);
                    // Expect at least 7 parts: type|param|amount|auto|date|category|note
                    if (parts.size() >= 7) {
                        Schedule s;
                        s.type = (parts[0] == "E") ? ScheduleType::EveryXDays : ScheduleType::MonthlyDay;
                        try { s.param = stoi(parts[1]); } catch (...) { cerr << "Warning: invalid schedule param\n"; continue; }
                        try { s.amount = stod(parts[2]); } catch (...) { cerr << "Warning: invalid schedule amount\n"; continue; }
                        s.autoAllocate = (parts[3] == "1" || parts[3] == "true");
                        chrono_tp nd;
                        if (!tryParseDate(parts[4], nd)) { cerr << "Warning: invalid schedule date '" << parts[4] << "'. Skipping schedule.\n"; continue; }
                        s.nextDate = nd;
                        s.category = parts[5];
                        s.note = parts[6];
                        if (s.type == ScheduleType::EveryXDays && s.param <= 0) { cerr << "Skipping schedule with non-positive interval\n"; continue; }
                        if (s.type == ScheduleType::MonthlyDay && (s.param < 1 || s.param > 31)) { cerr << "Skipping schedule with invalid day-of-month\n"; continue; }
                        schedules.push_back(s);
                    } else cerr << "Warning: invalid schedule line: " << line << "\n";
                } else if (sec == Txs) {
                    auto parts = splitEscaped(line);
                    if (parts.size() >= 4) {
                        Transaction t;
                        chrono_tp dt;
                        if (!tryParseDate(parts[0], dt)) { cerr << "Warning: invalid tx date '" << parts[0] << "'. Skipping tx.\n"; continue; }
                        t.date = dt;
                        try { t.amount = stod(parts[1]); } catch (...) { cerr << "Warning: invalid tx amount\n"; continue; }
                        t.category = parts[2];
                        t.note = parts[3];
                        txs.push_back(t);
                        string nk = normalizeKey(t.category);
                        if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = t.category;
                    } else cerr << "Warning: invalid tx line: " << line << "\n";
                }
            }
        }
        ifs.close();

        // Recompute categoryBalances from transactions to ensure consistency.
        map<string,double> recomputedCats;
        for (auto &t : txs) {
            string nk = normalizeKey(t.category);
            if (recomputedCats.find(nk) == recomputedCats.end()) recomputedCats[nk] = 0.0;
            recomputedCats[nk] += t.amount;
            if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = t.category;
        }
        for (auto &p : categoryBalances) {
            if (recomputedCats.find(p.first) == recomputedCats.end()) recomputedCats[p.first] = p.second;
        }
        categoryBalances.swap(recomputedCats);

        for (auto &p : allocationPct) {
            if (displayNames.find(p.first) == displayNames.end()) displayNames[p.first] = p.first;
            if (categoryBalances.find(p.first) == categoryBalances.end()) categoryBalances[p.first] = 0.0;
        }

        double computedBalance = 0.0;
        for (auto &t : txs) computedBalance += t.amount;
        if (!computedBalance && hadSavedBalance) {
            // If no transactions but had saved balance, honor savedBalance
            balance = savedBalance;
        } else {
            if (hadSavedBalance && fabs(savedBalance - computedBalance) > 0.01) {
                cerr << "Warning: saved BALANCE (" << fixed << setprecision(2) << savedBalance
                     << ") differs from recomputed (" << computedBalance << "). Using recomputed.\n";
            }
            balance = computedBalance;
        }

        // UI message: show in user's language loaded message in English (save file not localized)
        cout << "Loaded from " << filename << "\n";
        return true;
    }
};

// ============================================================
// SECTION 5B: INTERNATIONALIZATION & TRANSLATION SUPPORT
// ============================================================
// Multi-language support for UI strings
// Supported languages: EN (English), VI (Vietnamese), DE (German)
// Translation data loaded from ../config/i18n.h at compile time

#include "../config/i18n.h"

// tr(): Get translated message for a key in user's language
// If message contains {SAVE_FILENAME} placeholder, substitutes it at runtime
// Falls back to message key itself if translation not found
static inline string tr(const Settings &s, const string &id) {
    // Delegate to header-only i18n loader. If message contains placeholder {SAVE_FILENAME}
    // we'll substitute it here at runtime (so the header doesn't need to know about SAVE_FILENAME).
    string out = i18n.get(s.language, id);
    if (out.empty()) return id;
    const string placeholder = "{SAVE_FILENAME}";
    size_t pos = 0;
    while ((pos = out.find(placeholder, pos)) != string::npos) {
        out.replace(pos, placeholder.size(), SAVE_FILENAME);
        pos += SAVE_FILENAME.size();
    }
    return out;
}

// ============================================================
// SECTION 6: USER INTERFACE & MENU FUNCTIONS
// ============================================================
// Menu display, user interaction, and translations
// All text output uses tr() for multi-language support (EN, VI, DE)

// ---- ANSI terminal helpers (used by menu functions) ----

#ifdef _WIN32
inline void clearScreen() {
    // Use simple ANSI clear screen + cursor home (works with alternate screen buffer)
    // \033[2J = clear screen, \033[H = cursor to home (1,1)
    cout << "\033[2J\033[H" << flush;
}
#else
inline void clearScreen() {
    // noop on non-Windows
}
#endif

// Legacy name for compatibility
inline void clearScreenAndScrollbackWindows() {
    clearScreen();
}

inline void initTerminalANSI() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return;

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, mode);
#endif
}

inline void enterAlternateScreen() {
    std::cout << "\033[?1049h" << std::flush;
}

inline void exitAlternateScreen() {
    std::cout << "\033[?1049l" << std::flush;
}

// ---- Menu displays ----
// Display starting guide with all features and how-to instructions
void printStartingGuide(const Settings &s) {
    clearScreenAndScrollbackWindows();
    cout << tr(s, "starting_guide_title");
    cout << tr(s, "guide_H");
    cout << tr(s, "guide_1");
    cout << tr(s, "guide_2");
    cout << tr(s, "guide_3");
    cout << tr(s, "guide_4");
    cout << tr(s, "guide_5");
    cout << tr(s, "guide_6");
    cout << tr(s, "guide_7");
    cout << tr(s, "guide_8");
    cout << tr(s, "guide_9");
    cout << tr(s, "guide_10");
    cout << tr(s, "guide_return");
    cout << tr(s, "press_enter") << "\n";
}

// Display main menu with all available options
void printMenu(const Settings &s) {
    clearScreenAndScrollbackWindows();
    cout << "\n" << tr(s, "menu_title") << "\n";
    cout << tr(s, "menu_H") << "\n";
    cout << tr(s, "menu_1") << "\n";
    cout << tr(s, "menu_2") << "\n";
    cout << tr(s, "menu_3") << "\n";
    cout << tr(s, "menu_4") << "\n";
    cout << tr(s, "menu_5") << "\n";
    cout << tr(s, "menu_6") << "\n";
    cout << tr(s, "menu_7") << "\n";
    cout << tr(s, "menu_8") << "\n";
    cout << tr(s, "menu_9") << "\n";
    cout << tr(s, "menu_10") << "\n";
    cout << tr(s, "choice");
}

// ---- String utilities ----
// Trim leading and trailing whitespace from string in-place
static inline void trim_inplace(string &s) {
    while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
}

// ---- Interactive user input helpers ----
// Prompt user to return to menu or save and exit
// Returns true to continue, false to exit program
static inline bool askReturnToMenuOrSave(Account &acc) {
    cout << tr(acc.settings, "saved_exit_prompt");
    string resp;
    if (!getline(cin, resp)) return false;
    trim_inplace(resp);
    if (resp.empty()) return true; // just return to menu
    if (resp[0] == 's' || resp[0] == 'S') {
        acc.saveToFile();
        cout << tr(acc.settings, "saved_and_exiting") << "\n";
        return false;
    }
    // For any other input, treat as return to menu (less likely to accidentally exit)
    return true;
}

// Interactive allocation percentage setup
// Allows user to adjust how income is distributed across categories
// "Other" category gets the remainder (100 - sum of other categories)
// Rejects invalid inputs (negative, >100), shows running total
// Used for both initial setup and later modifications
void interactiveAllocSetup(Account &acc, bool initialSetup = false) {
    // Build list of categories excluding "Other"
    vector<string> keys; // normalized keys
    string nkOther = normalizeKey("Other");
    for (auto &p : acc.displayNames) {
        if (p.first == nkOther) continue;
        keys.push_back(p.first);
    }
    sort(keys.begin(), keys.end(), [&](const string &a, const string &b){
        return acc.displayNames[a] < acc.displayNames[b];
    });

    map<string, double> newPct = acc.allocationPct; // start from existing

    while (true) {
        double sumAssigned = 0.0;
        for (auto &k : keys) {
            double cur = 0.0;
            if (newPct.find(k) != newPct.end()) cur = newPct[k];
            sumAssigned += cur;
        }
        double remaining = 100.0 - sumAssigned;
        if (remaining < 0) remaining = 0.0;

        cout << tr(acc.settings, "alloc_intro") << "\n";
        cout << tr(acc.settings, "alloc_note") << "\n";
        {
            std::ostringstream oss; oss << fixed << setprecision(2) << remaining;
            std::string s = tr(acc.settings, "alloc_remaining");
            size_t pos = s.find("{PCT}");
            if (pos != std::string::npos) s.replace(pos, 5, oss.str());
            cout << s << "\n";
        }

        map<string, double> attempted = newPct; // copy to modify inline
        bool anyChange = false;

        for (auto &k : keys) {
            string display = acc.displayNames[k];
            double cur = 0.0;
            if (attempted.find(k) != attempted.end()) cur = attempted[k];

            // per-category input loop to reject negative or >100 values
            while (true) {
                {
                    std::ostringstream __oss; __oss << fixed << setprecision(0) << cur;
                    string s = tr(acc.settings, "alloc_prompt_current_percent");
                    size_t __pos;
                    if ((__pos = s.find("{NAME}")) != string::npos) s.replace(__pos, 6, display);
                    if ((__pos = s.find("{PCT}")) != string::npos) s.replace(__pos, 5, __oss.str());
                    cout << s;
                }
                string line;
                if (!getline(cin, line)) line.clear();
                trim_inplace(line);
                if (line.empty()) break; // keep current
                double val;
                try {
                    size_t pos = 0;
                    val = stod(line, &pos);
                    if (pos != line.size()) { cout << tr(acc.settings, "invalid_input_extra") << "\n"; continue; }
                } catch (...) {
                    cout << tr(acc.settings, "invalid_input") << "\n";
                    continue;
                }
                if (!isfinite(val)) { cout << tr(acc.settings, "invalid_number") << "\n"; continue; }
                if (val < 0.0) { cout << tr(acc.settings, "invalid_negative_percent") << "\n"; continue; }
                if (val > 100.0) { cout << tr(acc.settings, "invalid_percent_over") << "\n"; continue; }
                attempted[k] = val;
                anyChange = true;
                break;
            }
        }

        // Validate totals
        double total = 0.0;
        for (auto &k : keys) total += attempted[k];
        if (total < -1e-9 || total > 100.0 + 1e-9) {
            std::ostringstream oss; oss << fixed << setprecision(2) << total;
            std::string s = tr(acc.settings, "invalid_alloc_sum");
            size_t pos = s.find("{TOTAL}");
            if (pos != std::string::npos) s.replace(pos, 7, oss.str());
            cout << s << "\n";
            // loop again
            continue;
        }
        // Everything valid: set allocations
        for (auto &k : keys) acc.allocationPct[k] = attempted[k];
        // Set Other to remainder
        double sumNow = 0.0;
        for (auto &k : keys) sumNow += acc.allocationPct[k];
        double otherPct = 100.0 - sumNow;
        if (otherPct < 0) otherPct = 0;
        string nkOther = normalizeKey("Other");
        acc.allocationPct[nkOther] = otherPct;
        // Ensure displayName exists for Other
        if (acc.displayNames.find(nkOther) == acc.displayNames.end()) acc.displayNames[nkOther] = "Other";
        {
            std::ostringstream oss; oss << fixed << setprecision(2) << acc.allocationPct[nkOther];
            std::string s = tr(acc.settings, "allocations_updated");
            size_t pos = s.find("{PCT}");
            if (pos != std::string::npos) s.replace(pos, 5, oss.str());
            cout << s << "\n";
        }
        break;
    }
}

// Helper: interactive category creation for initial setup
void interactiveCategorySetup(Account &acc) {
    cout << tr(acc.settings, "category_setup_header") << "\n";
    cout << tr(acc.settings, "category_setup_prompt") << "\n";
    cout << tr(acc.settings, "category_setup_defaults") << "\n";
    vector<string> newCats;
    while (true) {
        cout << tr(acc.settings, "prompt_category_name");
        string line;
        if (!getline(cin, line)) line.clear();
        trim_inplace(line);
        if (line.empty()) break;
        string sanitized = sanitizeDisplayName(line);
        newCats.push_back(sanitized);
    }
    if (newCats.empty()) {
        cout << tr(acc.settings, "no_categories_entered") << "\n";
        acc = Account(); // defaults already set
        return;
    }
    // Reset account categories to only these plus Other
    acc.displayNames.clear();
    acc.categoryBalances.clear();
    acc.allocationPct.clear();
    for (auto &d : newCats) {
        string nk = normalizeKey(d);
        acc.displayNames[nk] = d;
        acc.categoryBalances[nk] = 0.0;
        acc.allocationPct[nk] = 0.0;
    }
    // Ensure Other exists
    string nkOther = normalizeKey("Other");
    if (acc.displayNames.find(nkOther) == acc.displayNames.end()) {
        acc.displayNames[nkOther] = "Other";
        acc.categoryBalances[nkOther] = 0.0;
        acc.allocationPct[nkOther] = 0.0;
    }
    cout << tr(acc.settings, "categories_created") << "\n";
}

// Called from main when user chooses to set up new account
void runInitialSetup(Account &acc) {
    interactiveCategorySetup(acc);
    cout << tr(acc.settings, "prompt_setup_alloc");
    string resp;
    if (!getline(cin, resp)) resp = "l";
    trim_inplace(resp);
    if (!resp.empty() && (resp[0]=='s' || resp[0]=='S')) {
        interactiveAllocSetup(acc, true);
    } else {
        cout << tr(acc.settings, "setup_alloc_later") << "\n";
    }
    cout << tr(acc.settings, "setup_complete") << "\n";
}

// ---- Configuration menus ----
// Interactive settings menu for user preferences
// Allows toggling: auto-save, auto-process on startup
// Allows setting: language (EN, VI, DE)
void settingsMenu(Account &acc) {
    // Ensure input buffer is clean at entry
    // (we use getline consistently; this reduces leftover newline problems)
    while (true) {
        clearScreenAndScrollbackWindows();
        cout << "\n--- Settings ---\n";
        // Display exactly as requested with numbered toggles and language block
        // 1. Auto-save: OFF
        // 2. Auto process schedules & interest at startup: OFF
        // 3. Language: EN
        //     Available languages:
        //     - EN
        //     - VI
        // (n) Nuke program (reset and delete save file)
        // Press Enter to go back to main menu.
            cout << tr(acc.settings, "label_auto_save") << (acc.settings.autoSave ? tr(acc.settings, "on") : tr(acc.settings, "off")) << "\n";
            cout << tr(acc.settings, "label_auto_process") << (acc.settings.autoProcessOnStartup ? tr(acc.settings, "on") : tr(acc.settings, "off")) << "\n";
            cout << tr(acc.settings, "label_language") << acc.settings.language << "\n";
            cout << "\t" << tr(acc.settings, "available_languages") << "\n";
            {
                auto langs = i18n.availableLanguages();
                sort(langs.begin(), langs.end(), [](auto &a, auto &b){ return a.second < b.second; });
                string cur = acc.settings.language;
                for (size_t i = 0; i < langs.size(); ++i) {
                    if (langs[i].first == cur) { auto m = langs[i]; langs.erase(langs.begin() + i); langs.insert(langs.begin(), m); break; }
                }
                for (auto &p : langs) {
                    string marker = (p.first == cur) ? string(" ") + tr(acc.settings, "current_marker") : string();
                    cout << "\t- " << p.first << " - " << p.second << marker << "\n";
                }
            }
            cout << tr(acc.settings, "nuke_desc") << "\n";
            cout << tr(acc.settings, "press_enter") << "\n";
            cout << tr(acc.settings, "choice");

        string ch;
        if (!getline(cin, ch)) ch.clear();
        trim_inplace(ch);
        if (ch.empty()) return; // single Enter returns immediately
        // Allow user to input number or letter
        if (ch == "1") {
            acc.settings.autoSave = !acc.settings.autoSave;
            cout << tr(acc.settings, "auto_save_changed") << (acc.settings.autoSave ? tr(acc.settings, "on") : tr(acc.settings, "off")) << ".\n";
        } else if (ch == "2") {
            acc.settings.autoProcessOnStartup = !acc.settings.autoProcessOnStartup;
            cout << tr(acc.settings, "auto_process_changed") << (acc.settings.autoProcessOnStartup ? tr(acc.settings, "on") : tr(acc.settings, "off")) << ".\n";
        } else if (ch == "3") {
            // Show available languages and allow picking by number or code
            auto langs = i18n.availableLanguages();
            cout << "Available languages:\n";
            for (size_t i = 0; i < langs.size(); ++i) {
                cout << "\t" << (i+1) << ") " << langs[i].first << " - " << langs[i].second << "\n";
            }
            cout << tr(acc.settings, "choose_language_prompt") << " ";
            string langsel;
            if (!getline(cin, langsel)) langsel.clear();
            trim_inplace(langsel);
            if (!langsel.empty()) {
                bool changed = false;
                // number
                bool isnum = true;
                for (char ch2 : langsel) if (!isdigit((unsigned char)ch2)) { isnum = false; break; }
                if (isnum) {
                    int idx = stoi(langsel) - 1;
                    if (idx >= 0 && idx < (int)langs.size()) {
                        acc.settings.language = langs[idx].first;
                        changed = true;
                    }
                } else {
                    // uppercase code
                    string code = langsel;
                    for (auto &c : code) c = (char)toupper((unsigned char)c);
                    for (auto &p : langs) {
                        if (p.first == code) { acc.settings.language = code; changed = true; break; }
                    }
                }
                if (changed) {
                    std::string s = tr(acc.settings, "language_set");
                    size_t pos = s.find("{LANG}");
                    if (pos != std::string::npos) s.replace(pos, 6, acc.settings.language);
                    cout << s << "\n";
                } else {
                    cout << tr(acc.settings, "invalid_choice") << "\n";
                }
            }
        } else {
            // handle n/N for nuke
            char c = ch[0];
            if (c == 'n' || c == 'N') {
                cout << tr(acc.settings, "nuke_confirm");
                string resp;
                if (!getline(cin, resp)) resp.clear();
                trim_inplace(resp);
                if (!resp.empty() && (resp[0]=='y' || resp[0]=='Y')) {
                    // reset in-memory account
                    acc = Account();

                    // try to remove save file
                    if (remove(SAVE_FILENAME.c_str()) == 0) {
                        cout << tr(acc.settings, "save_file_removed") << "\n";
                    } else {
                        // reuse translation helper for message
                        cout << tr(acc.settings, "no_save_file") << "\n";
                    }

                    // notify user and exit immediately
                    cout << tr(acc.settings, "nuke_done") << "\n";
                    cout << tr(acc.settings, "exiting_program") << "\n";

                    cout.flush(); // ensure messages are printed
                    exit(0);     // force immediate termination
                } else {
                    cout << tr(acc.settings, "nuke_cancel") << "\n";
                }
            } else {
                cout << tr(acc.settings, "unknown_option") << "\n";
            }
        }
    }
}


// ============================================================
// SECTION 7: MAIN ENTRY POINT & INTERACTIVE LOOP
// ============================================================
// Program initialization, menu loop, and command dispatch
// Handles: working directory setup, user input parsing, feature execution

int main(int argc, char **argv) {

    // Enable ANSI escape sequences
    initTerminalANSI();

    // Switch to alternate screen buffer for clean full-screen UI
    enterAlternateScreen();

    #include <filesystem>
    
    // Set up project root based on executable location
    // argv[0] points to the executable, which is in src/ or bin/
    std::filesystem::path exePath(argv[0]);
    try {
        exePath = std::filesystem::canonical(exePath);
    } catch (...) {
        exePath = std::filesystem::absolute(exePath);
    }
    // exe is in src/ or bin/, project root is the parent of that
    std::filesystem::path projectRoot = exePath.parent_path().parent_path();
    // Now change to project root so all relative paths work correctly
    std::filesystem::current_path(projectRoot);
    
    // Reload i18n now that working directory is correct
    i18n.reload();
    
    std::cout << "Working directory: " << std::filesystem::current_path() << '\n';

    // Helper: quick locale dump mode for automated checks
    if (argc == 3 && std::string(argv[1]) == "--dump-loc") {
        std::string code = argv[2];
        std::cout << "Dumping locale " << code << "\n";
        std::vector<std::string> keys = {"menu_title","prompt_date","saved_to","setup_complete"};
        for (auto &k : keys) std::cout << k << " -> '" << i18n.get(code,k) << "'\n";
        return 0;
    }

    // New helper to list loaded and skipped locale files (useful for debugging broken locales)
    if (argc == 2 && std::string(argv[1]) == "--list-locales") {
        auto d = i18n.getLoadDiagnostics();
        if (d.empty()) std::cout << "No locale diagnostics recorded.\n";
        for (auto &s : d) std::cout << s << "\n";
        // Also list recognized language codes
        std::cout << "\nAvailable locales (codes):\n";
        for (auto &p : i18n.availableLanguages()) std::cout << " - " << p.first << " : " << p.second << "\n";
        return 0;
    }

    // Non-interactive helper to dump the Settings display (useful for automated checks)
    if (argc == 2 && std::string(argv[1]) == "--dump-settings") {
        Account acc;
        (void)acc.loadFromFile(); // populate acc.settings if save exists
        cout << "\n--- Settings ---\n";
        cout << "1. Auto-save: " << (acc.settings.autoSave ? "ON" : "OFF") << "\n";
        cout << "2. Auto process schedules & interest at startup: " << (acc.settings.autoProcessOnStartup ? "ON" : "OFF") << "\n";
        cout << "3. Language: " << acc.settings.language << "\n";
        cout << "\t" << tr(acc.settings, "available_languages") << "\n";
        {
            auto langs = i18n.availableLanguages();
            sort(langs.begin(), langs.end(), [](auto &a, auto &b){ return a.second < b.second; });
            string cur = acc.settings.language;
            for (size_t i = 0; i < langs.size(); ++i) {
                if (langs[i].first == cur) { auto m = langs[i]; langs.erase(langs.begin() + i); langs.insert(langs.begin(), m); break; }
            }
            for (auto &p : langs) {
                string marker = (p.first == cur) ? string(" ") + tr(acc.settings, "current_marker") : string();
                cout << "\t- " << p.first << " - " << p.second << marker << "\n";
            }
        }
        cout << "(n) Nuke program (reset and delete save file)\n";
        cout << tr(acc.settings, "press_enter") << "\n";
        cout << tr(acc.settings, "choice");
        return 0;
    }

    ios::sync_with_stdio(false);
    cin.tie(&cout);
    Account acc;

    // Attempt to load; if missing, ask user whether to set up or retry
    bool loaded = acc.loadFromFile();
    if (!loaded) {
        cout << tr(acc.settings, "cannot_open_load") << "\n";
        while (true) {
            cout << tr(acc.settings, "choose_setup_or_retry");
            string resp;
            if (!getline(cin, resp)) { resp = "s"; }
            trim_inplace(resp);
            if (!resp.empty() && (resp[0]=='s' || resp[0]=='S')) {
                runInitialSetup(acc);
                break;
            } else if (!resp.empty() && (resp[0]=='r' || resp[0]=='R')) {
                cout << tr(acc.settings, "retrying_load") << "\n";
                if (acc.loadFromFile()) {
                    break;
                } else {
                    cout << tr(acc.settings, "still_no_save") << "\n";
                    continue;
                }
            } else {
                cout << tr(acc.settings, "please_answer_s_or_r") << "\n";
            }
        }
    } else {
        // If loaded and auto-process setting is enabled, run it now
        if (acc.settings.autoProcessOnStartup) {
            cout << tr(acc.settings, "auto_processing_start") << "\n";
            acc.processSchedulesUpTo(today());
            acc.applyInterestUpTo(today());
            cout << tr(acc.settings, "auto_processing_done") << "\n";
        }
    }

    while (true) {
        printMenu(acc.settings);
        string choiceStr;
        if (!getline(cin, choiceStr)) break;
        trim_inplace(choiceStr);
        if (choiceStr.empty()) continue;

        bool didExit = false;

        if (choiceStr == "H" || choiceStr == "h") {
            printStartingGuide(acc.settings);
        } else {
            int choice = -1;
            try { choice = stoi(choiceStr); } catch (...) {
                cout << tr(acc.settings, "invalid_choice") << "\n";
                if (!askReturnToMenuOrSave(acc)) break;
                else continue;
            }

            if (choice == 1) {
                // --- Add manual transaction (improved category selection) ---
                clearScreenAndScrollbackWindows();
                cout << tr(acc.settings, "prompt_date");
                string dateStr;
                if (!getline(cin, dateStr)) dateStr.clear();
                trim_inplace(dateStr);
                chrono_tp d;
                if (dateStr.empty()) d = today();
                else { if (!tryParseDate(dateStr, d)) { cout << tr(acc.settings, "invalid_date_format") << "\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; } }

                cout << tr(acc.settings, "prompt_amount");
                double amt;
                string amtLine;
                if (!getline(cin, amtLine)) amtLine.clear();
                trim_inplace(amtLine);
                try {
                    amt = stod(amtLine);
                } catch (...) {
                    cout << tr(acc.settings, "invalid_amount") << "\n";
                    if (!askReturnToMenuOrSave(acc)) break;
                    else continue;
                }

                vector<pair<string,string>> cats; // pair<display, normalized>
                for (auto &p : acc.displayNames) cats.emplace_back(p.second.empty() ? p.first : p.second, p.first);
                sort(cats.begin(), cats.end(), [](auto &a, auto &b){ return a.first < b.first; });

                cout << tr(acc.settings, "existing_categories") << "\n";
                for (size_t i = 0; i < cats.size(); ++i) {
                    cout << "  " << (i+1) << ". " << cats[i].first << "\n";
                }
                cout << tr(acc.settings, "prompt_category") ;
                string catInput;
                if (!getline(cin, catInput)) catInput.clear();
                trim_inplace(catInput);

                bool willAutoAllocate = false;
                string chosenDisplayCat;

                if (catInput.empty()) {
                    if (amt > 0.0) willAutoAllocate = true;
                    else chosenDisplayCat = "Other";
                } else {
                    bool handled = false;
                    bool isNumber = true;
                    for (char ch : catInput) if (!isdigit((unsigned char)ch)) { isNumber = false; break; }
                    if (isNumber) {
                        try {
                            int idx = stoi(catInput);
                            if (idx >= 1 && idx <= (int)cats.size()) {
                                chosenDisplayCat = cats[idx-1].first;
                                handled = true;
                            } else { cout << tr(acc.settings, "number_out_of_range") << "\n"; }
                        } catch (...) {}
                    }
                    if (!handled) {
                        string sanitized = sanitizeDisplayName(catInput);
                        string nk = normalizeKey(sanitized);
                        if (acc.displayNames.find(nk) != acc.displayNames.end()) {
                            chosenDisplayCat = acc.displayNames[nk];
                        } else {
                            while (true) {
                                string s = tr(acc.settings, "category_missing_prompt");
                                size_t pos = s.find("{NAME}");
                                if (pos != string::npos) s.replace(pos, 6, sanitized);
                                cout << s;
                                string resp; if (!getline(cin, resp)) resp = "r";
                                trim_inplace(resp);
                                if (!resp.empty() && (resp[0]=='c' || resp[0]=='C')) {
                                    acc.displayNames[nk] = sanitized;
                                    if (acc.categoryBalances.find(nk) == acc.categoryBalances.end()) acc.categoryBalances[nk] = 0.0;
                                    if (acc.allocationPct.find(nk) == acc.allocationPct.end()) acc.allocationPct[nk] = 0.0;
                                    chosenDisplayCat = sanitized;
                                    break;
                                } else if (!resp.empty() && (resp[0]=='r' || resp[0]=='R')) {
                                    cout << tr(acc.settings, "prompt_category");
                                    if (!getline(cin, catInput)) { catInput.clear(); }
                                    trim_inplace(catInput);
                                    if (catInput.empty()) {
                                        if (amt > 0.0) { willAutoAllocate = true; break; }
                                        else { chosenDisplayCat = "Other"; break; }
                                    }
                                    bool retriedHandled = false;
                                    bool retriedNumber = true;
                                    for (char ch : catInput) if (!isdigit((unsigned char)ch)) { retriedNumber = false; break; }
                                    if (retriedNumber) {
                                        try {
                                            int idx = stoi(catInput);
                                            if (idx >= 1 && idx <= (int)cats.size()) {
                                                chosenDisplayCat = cats[idx-1].first;
                                                retriedHandled = true;
                                            } else cout << tr(acc.settings, "number_out_of_range") << "\n";
                                        } catch (...) {}
                                    }
                                    if (retriedHandled) break;
                                    string sanitized2 = sanitizeDisplayName(catInput);
                                    string nk2 = normalizeKey(sanitized2);
                                    if (acc.displayNames.find(nk2) != acc.displayNames.end()) {
                                        chosenDisplayCat = acc.displayNames[nk2];
                                        break;
                                    } else {
                                        continue;
                                    }
                                } else {
                                    cout << tr(acc.settings, "please_answer_c_or_r") << "\n";
                                }
                            }
                        }
                    }
                }

                cout << tr(acc.settings, "prompt_note");
                string note; if (!getline(cin, note)) note.clear();

                if (amt > 0.0 && willAutoAllocate) {
                    acc.allocateAmount(d, amt, note + " (manual income)");
                    cout << tr(acc.settings, "added_auto_allocated") << "\n";
                } else {
                    if (chosenDisplayCat.empty()) chosenDisplayCat = "Other";
                    string nkChosen = normalizeKey(sanitizeDisplayName(chosenDisplayCat));
                    if (acc.displayNames.find(nkChosen) == acc.displayNames.end()) acc.displayNames[nkChosen] = sanitizeDisplayName(chosenDisplayCat);
                    acc.addManualTransaction(d, amt, acc.displayNames[nkChosen], note);
                    cout << tr(acc.settings, "added") << "\n";
                }

            } else if (choice == 2) {
                clearScreenAndScrollbackWindows();
                cout << tr(acc.settings, "schedule_type_prompt");
                string tline;
                if (!getline(cin, tline)) tline.clear();
                trim_inplace(tline);
                int t = 0;
                try { t = stoi(tline); } catch (...) { t = 0; }
                if (t != 1 && t != 2) {
                    cout << tr(acc.settings, "unknown_option") << "\n";

                    if (!askReturnToMenuOrSave(acc)) break;
                    else continue;
                }

                Schedule s;
                if (t == 1) {
                    s.type = ScheduleType::EveryXDays;
                    cout << tr(acc.settings, "prompt_interval_days");
                    string p; if (!getline(cin, p)) p.clear();
                    trim_inplace(p);
                    try { s.param = stoi(p); } catch (...) { s.param = 0; }
                    if (s.param <= 0) { cout << tr(acc.settings, "interval_must_positive") << "\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; }
                } else {
                    s.type = ScheduleType::MonthlyDay;
                    cout << tr(acc.settings, "prompt_day_of_month");
                    string p; if (!getline(cin, p)) p.clear();
                    trim_inplace(p);
                    try { s.param = stoi(p); } catch (...) { s.param = 0; }
                    if (s.param < 1 || s.param > 31) { cout << tr(acc.settings, "number_out_of_range") << "\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; }
                }

                cout << tr(acc.settings, "prompt_amount_recurring");
                string amtLine;
                if (!getline(cin, amtLine)) amtLine.clear();
                trim_inplace(amtLine);
                try { s.amount = stod(amtLine); } catch (...) { cout << tr(acc.settings, "invalid_amount") << "\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; }

                cout << tr(acc.settings, "prompt_note");
                if (!getline(cin, s.note)) s.note.clear();

                vector<pair<string,string>> cats; // pair<display, normalized>
                for (auto &p : acc.displayNames) cats.emplace_back(p.second.empty() ? p.first : p.second, p.first);
                sort(cats.begin(), cats.end(), [](auto &a, auto &b){ return a.first < b.first; });

                cout << tr(acc.settings, "prompt_category_info") << "\n";
                cout << tr(acc.settings, "existing_categories") << "\n";
                for (size_t i = 0; i < cats.size(); ++i) {
                    cout << "  " << (i+1) << ". " << cats[i].first << "\n";
                }
                cout << tr(acc.settings, "prompt_category") ;

                string catInput; if (!getline(cin, catInput)) catInput.clear();
                trim_inplace(catInput);

                s.category.clear();
                s.autoAllocate = false;

                if (catInput.empty()) {
                    if (s.amount > 0.0) {
                        s.autoAllocate = true;
                        s.category.clear();
                    } else {
                        s.category = "Other";
                        s.autoAllocate = false;
                    }
                } else {
                    bool isNumber = true;
                    for (char ch : catInput) if (!isdigit((unsigned char)ch)) { isNumber = false; break; }
                    bool handled = false;
                    if (isNumber) {
                        try {
                            int idx = stoi(catInput);
                            if (idx >= 1 && idx <= (int)cats.size()) {
                                s.category = cats[idx-1].first;
                                handled = true;
                            } else {
                                cout << tr(acc.settings, "number_out_of_range") << "\n";
                            }
                        } catch (...) {}
                    }
                    if (!handled) {
                        string sanitized = sanitizeDisplayName(catInput);
                        string nk = normalizeKey(sanitized);
                        if (acc.displayNames.find(nk) != acc.displayNames.end()) {
                            s.category = acc.displayNames[nk];
                        } else {
                            while (true) {
                                string promptStr = tr(acc.settings, "category_missing_prompt");
                                size_t pos = promptStr.find("{NAME}");
                                if (pos != string::npos) promptStr.replace(pos, 6, sanitized);
                                cout << promptStr;
                                string resp; if (!getline(cin, resp)) resp = "r";
                                trim_inplace(resp);
                                if (!resp.empty() && (resp[0]=='c' || resp[0]=='C')) {
                                    acc.displayNames[nk] = sanitized;
                                    if (acc.categoryBalances.find(nk) == acc.categoryBalances.end()) acc.categoryBalances[nk] = 0.0;
                                    if (acc.allocationPct.find(nk) == acc.allocationPct.end()) acc.allocationPct[nk] = 0.0;
                                    s.category = sanitized;
                                    break;
                                } else if (!resp.empty() && (resp[0]=='r' || resp[0]=='R')) {
                                    cout << tr(acc.settings, "prompt_category");

                                    if (!getline(cin, catInput)) catInput.clear();
                                    trim_inplace(catInput);
                                    if (catInput.empty()) {
                                        if (s.amount > 0.0) { s.autoAllocate = true; s.category.clear(); break; }
                                        else { s.category = "Other"; break; }
                                    }
                                    bool retriedNumber = true;
                                    for (char ch : catInput) if (!isdigit((unsigned char)ch)) { retriedNumber = false; break; }
                                    if (retriedNumber) {
                                        try {
                                            int idx = stoi(catInput);
                                            if (idx >= 1 && idx <= (int)cats.size()) {
                                                s.category = cats[idx-1].first;
                                                break;
                                            } else cout << tr(acc.settings, "number_out_of_range") << "\n";
                                        } catch (...) {}
                                    }
                                    string sanitized2 = sanitizeDisplayName(catInput);
                                    string nk2 = normalizeKey(sanitized2);
                                    if (acc.displayNames.find(nk2) != acc.displayNames.end()) {
                                        s.category = acc.displayNames[nk2];
                                        break;
                                    }
                                } else {
                                    cout << tr(acc.settings, "please_answer_c_or_r") << "\n";
                                }
                            }
                        }
                    }
                }

                if (s.autoAllocate && s.amount < 0.0) {
                    cout << tr(acc.settings, "auto_allocate_note") << "\n";

                    if (s.category.empty()) s.category = "Other";
                    s.autoAllocate = false;
                }

                cout << tr(acc.settings, "prompt_date");

                string start; if (!getline(cin, start)) start.clear();
                trim_inplace(start);
                if (start.empty()) s.nextDate = today();
                else { if (!tryParseDate(start, s.nextDate)) { cout << tr(acc.settings, "invalid_date_format") << "\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; } }

                acc.addSchedule(s);
                cout << tr(acc.settings, "scheduled_added") << "\n";

            } else if (choice == 3) {
                clearScreenAndScrollbackWindows();
                acc.printSummary();

            } else if (choice == 4) {
                // New allocation setup: operate only on existing categories (Other is remainder)
                clearScreenAndScrollbackWindows();
                interactiveAllocSetup(acc, false);

            } else if (choice == 5) {
                clearScreenAndScrollbackWindows();
                acc.processSchedulesUpTo(today());
                cout << tr(acc.settings, "processed_schedules") << "\n";

            } else if (choice == 6) {
                // New flow: let user choose to (A)dd/Update interest entry, (R)emove, or (P)lay now apply interest up to today
                clearScreenAndScrollbackWindows();
                cout << tr(acc.settings, "interest_menu");

                string sub; if (!getline(cin, sub)) sub = "p";
                trim_inplace(sub);
                if (!sub.empty() && (sub[0]=='a' || sub[0]=='A')) {
                    // Add or update interest entries for one or more categories
                    // List categories:
                    vector<pair<string,string>> cats; // display, normalized
                    for (auto &p : acc.displayNames) cats.emplace_back(p.second.empty() ? p.first : p.second, p.first);
                    sort(cats.begin(), cats.end(), [](auto &a, auto &b){ return a.first < b.first; });
                    cout << tr(acc.settings, "existing_categories") << "\n";
                    for (size_t i = 0; i < cats.size(); ++i) {
                        cout << "  " << (i+1) << ". " << cats[i].first << "\n";
                    }
                    cout << tr(acc.settings, "prompt_interest_categories");

                    string catSel; if (!getline(cin, catSel)) catSel.clear();
                    trim_inplace(catSel);
                    if (catSel.empty()) { cout << tr(acc.settings, "no_categories_selected") << "\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; }
                    // split on commas
                    vector<string> selections;
                    {
                        string tmp; for (char c : catSel) {
                            if (c == ',') { trim_inplace(tmp); if (!tmp.empty()) selections.push_back(tmp); tmp.clear(); }
                            else tmp.push_back(c);
                        }
                        trim_inplace(tmp); if (!tmp.empty()) selections.push_back(tmp);
                    }
                    // resolve selections to normalized keys
                    vector<string> targets;
                    for (auto &sel : selections) {
                        string s = sel;
                        trim_inplace(s);
                        bool isNumber = true;
                        for (char ch : s) if (!isdigit((unsigned char)ch)) { isNumber = false; break; }
                        if (isNumber) {
                            try {
                                int idx = stoi(s);
                                if (idx >= 1 && idx <= (int)cats.size()) {
                                    targets.push_back(cats[idx-1].second);
                                    continue;
                                } else {
                                    cout << tr(acc.settings, "selection_out_of_range") << s << "\n";

                                    continue;
                                }
                            } catch (...) { continue; }
                        } else {
                            string sanitized = sanitizeDisplayName(s);
                            string nk = normalizeKey(sanitized);
                            if (acc.displayNames.find(nk) != acc.displayNames.end()) {
                                targets.push_back(nk);
                                continue;
                            } else {
                                cout << tr(acc.settings, "category_not_found_ignored") << s << " (-> " << sanitized << "). " << "\n";

                                continue;
                            }
                        }
                    }
                    if (targets.empty()) { cout << tr(acc.settings, "no_valid_categories_selected") << "\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; }

                    // Ask monthly or annual — require 'm' or 'a' (blank = default monthly)
                    bool monthly = true;
                    while (true) {
                        cout << tr(acc.settings, "monthly_or_annual_prompt");

                        string ma;
                        if (!getline(cin, ma)) ma.clear();
                        trim_inplace(ma);

                        if (ma.empty()) { // default to monthly on empty input
                            monthly = true;
                            break;
                        }
                        char c = ma[0];
                        if (c == 'm' || c == 'M') { monthly = true; break; }
                        if (c == 'a' || c == 'A') { monthly = false; break; }

                        // invalid input — prompt again
                        cout << tr(acc.settings, "invalid_choice_m_or_a") << "\n";
                        // loop back to ask again
                    }
                    // Ask for rate value
                    cout << tr(acc.settings, "prompt_interest_rate");

                    string rateIn; if (!getline(cin, rateIn)) rateIn.clear();
                    trim_inplace(rateIn);
                    double ratePct = 0.0;
                    if (!tryParseRate(rateIn, ratePct)) {
                        cout << tr(acc.settings, "invalid_rate_input") << "\n";
                        if (!askReturnToMenuOrSave(acc)) break; else continue;
                    }
                    // Ask for start date
                    cout << tr(acc.settings, "prompt_date");

                    string startIn; if (!getline(cin, startIn)) startIn.clear();
                    trim_inplace(startIn);
                    chrono_tp startDate = today();
                    if (!startIn.empty()) {
                        if (!tryParseDate(startIn, startDate)) {
                            cout << tr(acc.settings, "invalid_date_format") << "\n";
                            if (!askReturnToMenuOrSave(acc)) break; else continue;
                        }
                    }
                    // For each target, set or overwrite interest entry
                    for (string nk : targets) {
                        // ensure displayName exists
                        if (acc.displayNames.find(nk) == acc.displayNames.end()) {
                            // create display name from normalized key (capitalize first letter)
                            string disp = sanitizeDisplayName(nk);
                            acc.displayNames[nk] = disp;
                            if (acc.categoryBalances.find(nk) == acc.categoryBalances.end()) acc.categoryBalances[nk] = 0.0;
                        }
                        InterestEntry ie;
                        ie.categoryNormalized = nk;
                        ie.ratePct = ratePct;
                        ie.monthly = monthly;
                        ie.startDate = startDate;
                        ie.lastAppliedDate = startDate; // no prior application
                        acc.interestMap[nk] = ie;
                        {
                            string s = tr(acc.settings, "interest_set_for");
                            string freq = monthly ? tr(acc.settings, "monthly") : tr(acc.settings, "annual");
                            size_t p;
                            if ((p = s.find("{FREQ}")) != string::npos) s.replace(p, 6, freq);
                            if ((p = s.find("{RATE}")) != string::npos) s.replace(p, 6, to_string(ratePct));
                            if ((p = s.find("{NAME}")) != string::npos) s.replace(p, 6, acc.displayNames[nk]);
                            if ((p = s.find("{DATE}")) != string::npos) s.replace(p, 6, toDateString(startDate));
                            cout << s << "\n";
                        }

                    }
                } else if (!sub.empty() && (sub[0]=='r' || sub[0]=='R')) {
                    // remove interest entries
                    if (acc.interestMap.empty()) { cout << tr(acc.settings, "no_interest_entries") << "\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; }
                    cout << tr(acc.settings, "interest_entries") << "\n";
                    vector<pair<int,string>> idxToNk;
                    int i = 1;
                    for (auto &kv : acc.interestMap) {
                        string disp = acc.displayNames.count(kv.first) ? acc.displayNames[kv.first] : kv.first;
                        cout << "  " << i << ") " << disp << " : " << kv.second.ratePct << (kv.second.monthly ? "% monthly" : "% annual") << "\n";
                        idxToNk.push_back({i, kv.first});
                        ++i;
                    }
                    cout << tr(acc.settings, "enter_numbers_to_remove");

                    string rem; if (!getline(cin, rem)) rem.clear();
                    trim_inplace(rem);
                    if (rem.empty()) { cout << tr(acc.settings, "no_selection_made") << "\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; }
                    vector<string> tokens;
                    string tmp;
                    for (char c : rem) {
                        if (c == ',') { trim_inplace(tmp); if (!tmp.empty()) tokens.push_back(tmp); tmp.clear();}
                        else tmp.push_back(c);
                    }
                    trim_inplace(tmp); if (!tmp.empty()) tokens.push_back(tmp);
                    for (auto &t : tokens) {
                        try {
                            int idx = stoi(t);
                            for (auto &p : idxToNk) if (p.first == idx) {
                                acc.interestMap.erase(p.second);
                                {
                                    string sMsg = tr(acc.settings, "interest_removed_for");
                                    size_t pos = sMsg.find("{NAME}");
                                    string name = (acc.displayNames.count(p.second) ? acc.displayNames[p.second] : p.second);
                                    if (pos != string::npos) sMsg.replace(pos, 6, name);
                                    cout << sMsg << "\n";
                                }

                            }
                        } catch (...) {}
                    }
                } else {
                    // process / apply interest now
                    {
                        string s = tr(acc.settings, "interest_processing");
                        size_t p = s.find("{DATE}");
                        if (p != string::npos) s.replace(p, 6, toDateString(today()));
                        cout << s << "\n";
                    }

                    acc.applyInterestUpTo(today());
                    cout << tr(acc.settings, "interest_applied") << "\n";
                }

            } else if (choice == 7) {
                clearScreenAndScrollbackWindows();
                acc.saveToFile();

            } else if (choice == 8) {
                clearScreenAndScrollbackWindows();
                bool ok = acc.loadFromFile();
                if (!ok) {
                    cout << tr(acc.settings, "cannot_open_load");
                } else {
                    // If loaded and auto-process setting is enabled, run it now
                    if (acc.settings.autoProcessOnStartup) {
                        cout << tr(acc.settings, "auto_processing_start") << "\n";

                        acc.processSchedulesUpTo(today());
                        acc.applyInterestUpTo(today());
                        cout << tr(acc.settings, "auto_processing_done") << "\n";

                    }
                }

            } else if (choice == 9) {
                clearScreenAndScrollbackWindows();
                cout << tr(acc.settings, "goodbye") << "\n";
                didExit = true;
            } else if (choice == 10) {
                // Enter settings. settingsMenu uses getline internally so no extra newline issues.
                // (settingsMenu already has clearScreenAndScrollbackWindows at its start)
                settingsMenu(acc);
            } else {
                cout << tr(acc.settings, "invalid_choice") << "\n";
            }

            if (acc.settings.autoSave) {
                // auto-save after each action if enabled
                acc.saveToFile();
            }

            if (didExit) break;
        }

        if (!askReturnToMenuOrSave(acc)) break;
    }

    return 0;
}
