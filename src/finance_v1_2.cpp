// Finance Manager v1.2 - Personal Finance Management System
// 
// A comprehensive CLI-based personal finance manager with support for:
//   - Transaction management (manual income/expenses)
//   - Scheduled transactions (recurring payments)
//   - Category-based allocation with percentage distribution
//   - Interest calculations (monthly/annual rates)
//   - Settings management (auto-save, auto-processing, language preferences)
//   - Portable path resolution (works from any execution context)
//   - Atomic file saving with error recovery
//
// Architecture Overview:
//   1. Core Data Structures: Transaction, Schedule, InterestEntry, Settings, Account
//   2. Helper Utilities: Date/time, escaping, normalization, parsing
//   3. Account Management: In-memory data + persistent save/load
//   4. User Interface: Menu-driven CLI with multi-language support
//   5. Main Loop: Initialization, menu processing, and graceful shutdown
//
// Build: g++ -std=c++17 finance_v1_2.cpp -o finance_v1_2
// Usage: ./finance_v1_2 (interactive mode) or with helper flags (--dump-loc, --list-locales, etc.)
//
// Dependencies:
//   - C++17 (filesystem, chrono)

#include <bits/stdc++.h>
using namespace std;
using chrono_tp = chrono::system_clock::time_point;

static const string SAVE_FILENAME = "finance_save.txt";

struct Transaction {
    chrono_tp date;
    double amount; // positive = income, negative = expense
    string category;
    string note;
};

enum class ScheduleType { EveryXDays, MonthlyDay };

struct Schedule {
    ScheduleType type;
    int param; // days interval or day-of-month
    double amount;
    string note;
    bool autoAllocate;
    chrono_tp nextDate;
};

// -------------------- safe localtime --------------------
static inline tm safeLocaltime(time_t tt) {
    tm result{};
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
    // localtime_s(result, time_t*) signature (order: result, time)
    localtime_s(&result, &tt);
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    // POSIX thread-safe
    localtime_r(&tt, &result);
#else
    // Last resort: not thread-safe but copied immediately
    tm *tmp = localtime(&tt);
    if (tmp) result = *tmp;
#endif
    return result;
}

// -------------------- Date parsing/formatting --------------------
static inline bool tryParseDate(const string &s, chrono_tp &out) {
    if (s.empty()) return false;
    tm t = {};
    istringstream ss(s);
    ss >> get_time(&t, "%Y-%m-%d");
    if (ss.fail()) return false;
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
    t.tm_isdst = -1; // let mktime determine DST
    time_t tt = mktime(&t);
    if (tt == -1) return false;
    out = chrono::system_clock::from_time_t(tt);
    return true;
}

static inline string toDateString(const chrono_tp &tp) {
    time_t tt = chrono::system_clock::to_time_t(tp);
    tm t = safeLocaltime(tt);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
    return string(buf);
}

static inline chrono_tp today() {
    auto now = chrono::system_clock::now();
    time_t tt = chrono::system_clock::to_time_t(now);
    tm t = safeLocaltime(tt);
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0; t.tm_isdst = -1;
    return chrono::system_clock::from_time_t(mktime(&t));
}

static inline int daysInMonth(int year, int month) {
    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2) {
        bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        return 28 + (leap ? 1 : 0);
    }
    return mdays[month - 1];
}

// Calendar-aware addDays via tm normalization (safer across DST)
static inline chrono_tp addDays(const chrono_tp &tp, int days) {
    time_t tt = chrono::system_clock::to_time_t(tp);
    tm t = safeLocaltime(tt);
    t.tm_mday += days;
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0; t.tm_isdst = -1;
    time_t newt = mktime(&t);
    return chrono::system_clock::from_time_t(newt);
}

// Next monthly on 'day' clamped to month length
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

// -------------------- Escaping helpers --------------------
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

// -------------------- Account --------------------
struct Account {
    double balance = 0.0;
    vector<Transaction> txs;
    vector<Schedule> schedules;
    map<string, double> allocationPct;
    map<string, double> categoryBalances;
    double savingAnnualInterestPct = 0.0;

    Account() {
        allocationPct["Saving"] = 20.0;
        allocationPct["Emergency"] = 20.0;
        allocationPct["Entertainment"] = 10.0;
        allocationPct["Other"] = 50.0;
        for (auto &p : allocationPct) categoryBalances[p.first] = 0.0;
    }

    void addManualTransaction(const chrono_tp &date, double amount, const string &category, const string &note) {
        Transaction t{date, amount, category, note};
        txs.push_back(t);
        balance += amount;
        if (categoryBalances.find(category) == categoryBalances.end())
            categoryBalances[category] = 0.0;
        categoryBalances[category] += amount;
    }

    void setAllocation(const map<string,double> &newAlloc) {
        allocationPct = newAlloc;
        for (auto &p : allocationPct)
            if (categoryBalances.find(p.first) == categoryBalances.end())
                categoryBalances[p.first] = 0.0;
    }

    void allocateAmount(double amount, const string &note) {
        double totalPct = 0;
        for (auto &p : allocationPct) totalPct += p.second;
        if (totalPct <= 0.000001) {
            categoryBalances["Other"] += amount;
            txs.push_back({today(), amount, "Other", note + " (auto alloc fallback)"});
            balance += amount;
            return;
        }
        for (auto &p : allocationPct) {
            double share = amount * (p.second / totalPct);
            categoryBalances[p.first] += share;
            txs.push_back({today(), share, p.first, note + " (auto alloc)"});
            balance += share;
        }
    }

    void addSchedule(const Schedule &s) {
        schedules.push_back(s);
    }

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
                if (s.autoAllocate) allocateAmount(s.amount, "Scheduled income: " + s.note);
                else addManualTransaction(s.nextDate, s.amount, "Other", "Scheduled income: " + s.note);

                if (s.type == ScheduleType::EveryXDays) s.nextDate = addDays(s.nextDate, s.param);
                else s.nextDate = nextMonthlyOn(s.nextDate, s.param);
                ++guard;
            }
            if (guard >= 10000) {
                cerr << "Warning: schedule processing hit guard limit for a schedule. Skipping further iterations for safety.\n";
            }
        }
    }

    void applyMonthlySavingInterest() {
        if (savingAnnualInterestPct <= 0.0) return;
        double monthlyRate = savingAnnualInterestPct / 100.0 / 12.0;
        double savingBal = 0.0;
        auto it = categoryBalances.find("Saving");
        if (it != categoryBalances.end()) savingBal = it->second;
        if (savingBal <= 0.0) return;
        double interest = savingBal * monthlyRate;
        categoryBalances["Saving"] += interest;
        txs.push_back({today(), interest, "Saving", "Monthly interest"});
        balance += interest;
    }

    void printSummary() {
        cout << "==== Account Summary ====\n";
        cout << "Total balance: " << fixed << setprecision(2) << balance << "\n";
        cout << "Category balances:\n";
        for (auto &p : categoryBalances)
            cout << "  - " << p.first << ": " << fixed << setprecision(2) << p.second << "\n";
        cout << "Allocations (%):\n";
        for (auto &p : allocationPct)
        
            cout << "  - " << p.first << ": " << p.second << "%\n";
        cout << "Scheduled incomes: " << schedules.size() << "\n";
        for (size_t i = 0; i < schedules.size(); ++i) {
            auto &s = schedules[i];
            cout << "  [" << i << "] amount=" << s.amount << " next=" << toDateString(s.nextDate)
                 << " type=" << (s.type==ScheduleType::EveryXDays? "EveryXDays":"MonthlyDay")
                 << " param=" << s.param << " autoAlloc=" << (s.autoAllocate? "yes":"no")
                 << " note=" << s.note << "\n";
        }
        cout << "Recent transactions (last 10):\n";
        int start = max(0, (int)txs.size()-10);
        for (int i = (int)txs.size()-1; i >= start; --i)
            cout << toDateString(txs[i].date) << " | " << setw(8) << txs[i].amount
                 << " | " << txs[i].category << " | " << txs[i].note << "\n";
        cout << "=========================\n";
    }

    void saveToFile(const string &filename = SAVE_FILENAME) {
        ofstream ofs(filename);
        if (!ofs) { cerr << "Cannot open file to save.\n"; return; }
        ofs << fixed << setprecision(10);
        ofs << "BALANCE " << balance << "\n";
        ofs << "SAVING_INTEREST " << savingAnnualInterestPct << "\n";
        ofs << "ALLOCATIONS\n";
        for (auto &p : allocationPct) ofs << escapeForSave(p.first) << "|" << p.second << "\n";
        ofs << "CATEGORIES\n";
        for (auto &p : categoryBalances) ofs << escapeForSave(p.first) << "|" << p.second << "\n";
        ofs << "SCHEDULES\n";
        for (auto &s : schedules) {
            ofs << (s.type==ScheduleType::EveryXDays? "E":"M") << "|"
                << s.param << "|" << s.amount << "|"
                << (s.autoAllocate ? "1" : "0") << "|" << escapeForSave(toDateString(s.nextDate)) << "|" << escapeForSave(s.note) << "\n";
        }
        ofs << "TXS\n";
        for (auto &t : txs) {
            ofs << escapeForSave(toDateString(t.date)) << "|" << t.amount << "|" << escapeForSave(t.category) << "|" << escapeForSave(t.note) << "\n";
        }
        ofs.close();
        cout << "Saved to " << filename << "\n";
    }

    void loadFromFile(const string &filename = SAVE_FILENAME) {
        ifstream ifs(filename);
        if (!ifs) { cerr << "Cannot open file to load. Starting fresh.\n"; return; }
        string line;
        enum Section { None, Alloc, Cats, Scheds, Txs } sec = None;
        allocationPct.clear(); categoryBalances.clear(); schedules.clear(); txs.clear();

        double savedBalance = 0.0;
        bool hadSavedBalance = false;

        while (getline(ifs, line)) {
            if (line == "ALLOCATIONS") { sec = Alloc; continue; }
            if (line == "CATEGORIES") { sec = Cats; continue; }
            if (line == "SCHEDULES") { sec = Scheds; continue; }
            if (line == "TXS") { sec = Txs; continue; }
            if (line.rfind("BALANCE ", 0) == 0) {
                try { savedBalance = stod(line.substr(8)); hadSavedBalance = true; } catch (...) { cerr << "Warning: invalid BALANCE value.\n"; }
            } else if (line.rfind("SAVING_INTEREST ", 0) == 0) {
                try { savingAnnualInterestPct = stod(line.substr(15)); } catch (...) { cerr << "Warning: invalid SAVING_INTEREST value.\n"; }
            } else {
                if (sec == Alloc) {
                    auto parts = splitEscaped(line);
                    if (parts.size() >= 2) {
                        string k = parts[0];
                        double v = 0.0;
                        try { v = stod(parts[1]); } catch (...) { cerr << "Warning: invalid allocation for " << k << "\n"; continue; }
                        allocationPct[k] = v;
                    } else cerr << "Warning: invalid allocation line: " << line << "\n";
                } else if (sec == Cats) {
                    auto parts = splitEscaped(line);
                    if (parts.size() >= 2) {
                        string k = parts[0];
                        double v = 0.0;
                        try { v = stod(parts[1]); } catch (...) { cerr << "Warning: invalid category balance for " << k << "\n"; continue; }
                        categoryBalances[k] = v;
                    } else cerr << "Warning: invalid category line: " << line << "\n";
                } else if (sec == Scheds) {
                    auto parts = splitEscaped(line);
                    if (parts.size() >= 6) {
                        Schedule s;
                        s.type = (parts[0] == "E") ? ScheduleType::EveryXDays : ScheduleType::MonthlyDay;
                        try { s.param = stoi(parts[1]); } catch (...) { cerr << "Warning: invalid schedule param\n"; continue; }
                        try { s.amount = stod(parts[2]); } catch (...) { cerr << "Warning: invalid schedule amount\n"; continue; }
                        s.autoAllocate = (parts[3] == "1" || parts[3] == "true");
                        chrono_tp nd;
                        if (!tryParseDate(parts[4], nd)) { cerr << "Warning: invalid schedule date '" << parts[4] << "'. Skipping schedule.\n"; continue; }
                        s.nextDate = nd;
                        s.note = parts[5];
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
                    } else cerr << "Warning: invalid tx line: " << line << "\n";
                }
            }
        }
        ifs.close();

        for (auto &p : allocationPct) if (categoryBalances.find(p.first) == categoryBalances.end()) categoryBalances[p.first] = 0.0;

        // Recompute balance from transactions (source of truth)
        double computedBalance = 0.0;
        for (auto &t : txs) computedBalance += t.amount;
        if (hadSavedBalance && fabs(savedBalance - computedBalance) > 0.01) {
            cerr << "Warning: saved BALANCE (" << fixed << setprecision(2) << savedBalance
                 << ") differs from recomputed (" << computedBalance << "). Using recomputed.\n";
        }
        balance = computedBalance;

        cout << "Loaded from " << filename << "\n";
    }
};

// -------------------- UI --------------------
void printStartingGuide() {
    cout << "\n=== Starting Guide ===\n";
    cout << "H) Starting Guide - show this help message.\n";
    cout << "1) Add manual transaction - date YYYY-MM-DD (empty = today), amount (+ income / - expense), category, note.\n";
    cout << "   If income, you can auto-allocate by your defined percentages.\n";
    cout << "2) Add scheduled income - recurring income every X days or monthly on day D.\n";
    cout << "3) Show summary - total balance, category balances, allocations, recent txs.\n";
    cout << "4) Set allocation percentages - define how income is split across categories.\n";
    cout << "5) Process schedules up to today - apply scheduled incomes that are due.\n";
    cout << "6) Apply monthly saving interest - set rate (annual %) and apply monthly interest.\n";
    cout << "7) Save - write current data to " << SAVE_FILENAME << ".\n";
    cout << "8) Load - load data from " << SAVE_FILENAME << ".\n";
    cout << "9) Exit - quit program.\n";
    cout << "10) Drop a nuke - resets program to initial state and deletes the save file (confirmation required).\n";
    cout << "Notes:\n";
    cout << "- Dates must be YYYY-MM-DD. Invalid dates will be rejected when parsed.\n";
    cout << "- Monthly schedule days are clamped to the month's length (31 -> 30 for April, etc.).\n";
    cout << "======================\n\n";
}

void printMenu() {
    cout << "\n=== Finance Manager ===\n";
    cout << "H) Starting Guide\n";
    cout << "1) Add manual transaction\n";
    cout << "2) Add scheduled income\n";
    cout << "3) Show summary\n";
    cout << "4) Set allocation percentages\n";
    cout << "5) Process schedules up to today\n";
    cout << "6) Apply monthly saving interest\n";
    cout << "7) Save\n";
    cout << "8) Load\n";
    cout << "9) Exit\n";
    cout << "10) Drop a nuke\n";
    cout << "Choice: ";
}

static inline void trim_inplace(string &s) {
    while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
}

static inline bool askReturnToMenu() {
    cout << "\nReturn to Main Interface? (y/N): ";
    string resp;
    if (!getline(cin, resp)) return false;
    trim_inplace(resp);
    if (!resp.empty() && (resp[0]=='y' || resp[0]=='Y')) return true;
    return false;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(&cout);
    Account acc;
    acc.loadFromFile();

    while (true) {
        printMenu();
        string choiceStr;
        if (!getline(cin, choiceStr)) break;
        trim_inplace(choiceStr);
        if (choiceStr.empty()) continue;

        bool didExit = false;

        if (choiceStr == "H" || choiceStr == "h") {
            printStartingGuide();
        } else {
            int choice = -1;
            try { choice = stoi(choiceStr); } catch (...) {
                cout << "Invalid choice.\n";
                if (!askReturnToMenu()) break;
                else continue;
            }

            if (choice == 1) {
                string dateStr;
                cout << "Date (YYYY-MM-DD) [empty = today]: ";
                getline(cin, dateStr);
                chrono_tp d;
                if (dateStr.empty()) d = today();
                else { if (!tryParseDate(dateStr, d)) { cout << "Invalid date format. Use YYYY-MM-DD.\n"; if (!askReturnToMenu()) didExit = true; } }

                if (didExit) break;

                cout << "Amount (positive = income, negative = expense): ";
                double amt; if (!(cin >> amt)) { cout << "Invalid amount.\n"; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); if (!askReturnToMenu()) break; else continue; }
                cin.ignore(numeric_limits<streamsize>::max(), '\n');

                cout << "Category: "; string cat; getline(cin, cat);
                if (cat.empty()) cat = "Other";

                cout << "Note: "; string note; getline(cin, note);

                bool autoAlloc = false;
                if (amt > 0.0) {
                    cout << "Auto-allocate by percentages? (y/n) [n]: ";
                    string tmp; getline(cin, tmp);
                    if (!tmp.empty() && (tmp[0]=='y' || tmp[0]=='Y')) autoAlloc = true;
                }

                if (amt > 0.0 && autoAlloc) acc.allocateAmount(amt, note + " (manual income)");
                else acc.addManualTransaction(d, amt, cat, note);
                cout << "Added.\n";

            } else if (choice == 2) {
                cout << "Type: 1) Every X days  2) Monthly on day D\nChoice: ";
                int t; if (!(cin >> t)) { cout << "Invalid type.\n"; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); if (!askReturnToMenu()) break; else continue; }
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                Schedule s;
                if (t == 1) {
                    s.type = ScheduleType::EveryXDays;
                    cout << "Enter days interval: "; if (!(cin >> s.param)) { cout << "Invalid interval.\n"; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); if (!askReturnToMenu()) break; else continue; }
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    if (s.param <= 0) { cout << "Interval must be > 0.\n"; if (!askReturnToMenu()) break; else continue; }
                } else {
                    s.type = ScheduleType::MonthlyDay;
                    cout << "Enter day of month (1-31): "; if (!(cin >> s.param)) { cout << "Invalid day.\n"; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); if (!askReturnToMenu()) break; else continue; }
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    if (s.param < 1 || s.param > 31) { cout << "Day must be between 1 and 31.\n"; if (!askReturnToMenu()) break; else continue; }
                }
                cout << "Amount (positive): "; if (!(cin >> s.amount)) { cout << "Invalid amount.\n"; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); if (!askReturnToMenu()) break; else continue; }
                cin.ignore(numeric_limits<streamsize>::max(), '\n');

                cout << "Note: "; getline(cin, s.note);

                cout << "Auto-allocate? (y/n) [y]: ";
                string tmp; getline(cin, tmp);
                s.autoAllocate = (tmp.empty() || tmp[0]=='y' || tmp[0]=='Y');

                cout << "Start date (YYYY-MM-DD) [today]: ";
                string start; getline(cin, start);
                if (start.empty()) s.nextDate = today();
                else { if (!tryParseDate(start, s.nextDate)) { cout << "Invalid start date.\n"; if (!askReturnToMenu()) break; else continue; } }
                acc.addSchedule(s);
                cout << "Scheduled income added.\n";

            } else if (choice == 3) {
                acc.printSummary();

            } else if (choice == 4) {
                cout << "Enter allocations as 'Category percent'. Empty line to finish.\n";
                map<string,double> newAlloc;
                while (true) {
                    cout << "Category percent> ";
                    string line; getline(cin, line);
                    if (line.empty()) break;
                    istringstream ss(line);
                    string cat; double pct;
                    if (!(ss >> cat >> pct)) { cout << "Invalid format. Example: Saving 30\n"; continue; }
                    newAlloc[cat] = pct;
                }
                if (!newAlloc.empty()) acc.setAllocation(newAlloc);
                cout << "Updated allocations.\n";

            } else if (choice == 5) {
                acc.processSchedulesUpTo(today());
                cout << "Processed schedules.\n";

            } else if (choice == 6) {
                cout << "Saving annual interest rate (%) [current " << acc.savingAnnualInterestPct << "]: ";
                string sline; getline(cin, sline);
                if (!sline.empty()) {
                    try { acc.savingAnnualInterestPct = stod(sline); } catch (...) { cout << "Invalid rate.\n"; if (!askReturnToMenu()) break; else continue; }
                }
                acc.applyMonthlySavingInterest();
                cout << "Interest applied.\n";

            } else if (choice == 7) {
                acc.saveToFile();

            } else if (choice == 8) {
                acc.loadFromFile();

            } else if (choice == 9) {
                cout << "Goodbye.\n";
                didExit = true;
            } else if (choice == 10) {
                cout << "Do you really want to nuke your program? (Everything will return to its initial state) [y/N]: ";
                string resp;
                getline(cin, resp);
                if (!resp.empty() && (resp[0]=='y' || resp[0]=='Y')) {
                    acc = Account();
                    if (remove(SAVE_FILENAME.c_str()) == 0) {
                        cout << "Save file removed.\n";
                    } else {
                        cout << "No save file to remove or failed to remove (it might not exist).\n";
                    }
                    cout << "Program nuked: returned to initial state.\n";
                } else {
                    cout << "Nuke cancelled.\n";
                }
            } else {
                cout << "Invalid choice.\n";
            }

            if (didExit) break;
        }

        // After finishing the action (including H and invalid choices),
        // ask whether to return to menu. Default is No -> exit.
        if (!askReturnToMenu()) break;
    }

    
    return 0;
}

