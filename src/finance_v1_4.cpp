// Finance Manager v1.4 - Personal Finance Management System
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
// Build: g++ -std=c++17 finance_v1_4.cpp -o finance_v1_4
// Usage: ./finance_v1_4 (interactive mode)

#include <bits/stdc++.h>
using namespace std;
using chrono_tp = chrono::system_clock::time_point;

static const string SAVE_FILENAME = "finance_save.txt";

struct Transaction {
    chrono_tp date;
    double amount; // positive = income, negative = expense
    string category; // display name
    string note;
};

enum class ScheduleType { EveryXDays, MonthlyDay };

// single Schedule struct (includes category)
struct Schedule {
    ScheduleType type;
    int param; // days interval or day-of-month
    double amount;
    string note;
    bool autoAllocate;
    chrono_tp nextDate;
    string category; // display name (may be empty => use Other or auto-alloc when appropriate)
};

// -------------------- safe localtime --------------------
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

// -------------------- Date parsing/formatting --------------------
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

static inline chrono_tp addDays(const chrono_tp &tp, int days) {
    time_t tt = chrono::system_clock::to_time_t(tp);
    tm t = safeLocaltime(tt);
    t.tm_mday += days;
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0; t.tm_isdst = -1;
    time_t newt = mktime(&t);
    return chrono::system_clock::from_time_t(newt);
}

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

// -------------------- Category normalization helper --------------------
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

// -------------------- Account --------------------
struct Account {
    double balance = 0.0;
    vector<Transaction> txs;
    vector<Schedule> schedules;
    map<string, double> allocationPct;      // normalized -> percent
    map<string, double> categoryBalances;   // normalized -> amount
    map<string, string> displayNames;       // normalized -> display name
    double savingAnnualInterestPct = 0.0;

    Account() {
        vector<pair<string,double>> defaults = {
            {"Saving", 20.0}, {"Emergency", 20.0}, {"Entertainment", 10.0}, {"Other", 50.0}
        };
        for (auto &p : defaults) {
            string nk = normalizeKey(p.first);
            allocationPct[nk] = p.second;
            displayNames[nk] = p.first;
            categoryBalances[nk] = 0.0;
        }
    }

    void addManualTransaction(const chrono_tp &date, double amount, const string &category, const string &note) {
        string catDisplay = category.empty() ? "Other" : category;
        string nk = normalizeKey(catDisplay);
        if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = catDisplay;
        Transaction t{date, amount, displayNames[nk], note};
        txs.push_back(t);
        balance += amount;
        if (categoryBalances.find(nk) == categoryBalances.end())
            categoryBalances[nk] = 0.0;
        categoryBalances[nk] += amount;
    }

    void setAllocation(const map<string,double> &newAlloc) {
        allocationPct.clear();
        for (auto &p : newAlloc) {
            string nk = normalizeKey(p.first);
            allocationPct[nk] = p.second;
            if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = p.first;
            if (categoryBalances.find(nk) == categoryBalances.end()) categoryBalances[nk] = 0.0;
        }
    }

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

    void addSchedule(const Schedule &s) {
        schedules.push_back(s);
    }

    // Updated to use schedule.category; supports negative amounts (expenses)
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

    void applyMonthlySavingInterest() {
        if (savingAnnualInterestPct <= 0.0) return;
        double monthlyRate = savingAnnualInterestPct / 100.0 / 12.0;
        double savingBal = 0.0;
        string nk = normalizeKey("Saving");
        auto it = categoryBalances.find(nk);
        if (it != categoryBalances.end()) savingBal = it->second;
        if (savingBal <= 0.0) return;
        double interest = savingBal * monthlyRate;
        categoryBalances[nk] += interest;
        chrono_tp dt = today();
        string display = displayNames[nk].empty() ? "Saving" : displayNames[nk];
        Transaction tt; tt.date = dt; tt.amount = interest; tt.category = display; tt.note = "Monthly interest";
        txs.push_back(tt);
        balance += interest;
    }

    void printSummary() {
        cout << "==== Account Summary ====\n";
        cout << "Total balance: " << fixed << setprecision(2) << balance << "\n";
        cout << "Category balances:\n";
        for (auto &p : categoryBalances) {
            string display = displayNames[p.first].empty() ? p.first : displayNames[p.first];
            cout << "  - " << display << ": " << fixed << setprecision(2) << p.second << "\n";
        }
        cout << "Allocations (%):\n";
        for (auto &p : allocationPct) {
            string display = displayNames[p.first].empty() ? p.first : displayNames[p.first];
            cout << "  - " << display << ": " << p.second << "%\n";
        }
        cout << "Scheduled transactions: " << schedules.size() << "\n";
        for (size_t i = 0; i < schedules.size(); ++i) {
            auto &s = schedules[i];
            cout << "  [" << i << "] amount=" << s.amount << " next=" << toDateString(s.nextDate)
                 << " type=" << (s.type==ScheduleType::EveryXDays? "EveryXDays":"MonthlyDay")
                 << " param=" << s.param << " autoAlloc=" << (s.autoAllocate? "yes":"no")
                 << " category=" << (s.category.empty() ? string("<<auto/Other>>") : s.category)
                 << " note=" << s.note << "\n";
        }
        cout << "Recent transactions (last 10):\n";
        int start = max(0, (int)txs.size()-10);
        for (int i = (int)txs.size()-1; i >= start; --i)
            cout << toDateString(txs[i].date) << " | " << setw(10) << txs[i].amount
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
        cout << "Saved to " << filename << "\n";
    }

    void loadFromFile(const string &filename = SAVE_FILENAME) {
        ifstream ifs(filename);
        if (!ifs) { cerr << "Cannot open file to load. Starting fresh.\n"; return; }
        string line;
        enum Section { None, Alloc, Cats, Scheds, Txs } sec = None;
        allocationPct.clear(); categoryBalances.clear(); schedules.clear(); txs.clear(); displayNames.clear();

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
    cout << "2) Add scheduled transaction - recurring every X days or monthly on day D.\n";
    cout << "3) Show summary - total balance, category balances, allocations, recent txs.\n";
    cout << "4) Set allocation percentages - define how income is split across categories.\n";
    cout << "5) Process schedules up to today - apply scheduled transactions that are due.\n";
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
    cout << "2) Add scheduled transaction\n";
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
    #include <filesystem>
    std::cout << "Working directory: " << std::filesystem::current_path() << '\n';

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
                // --- Add manual transaction (improved category selection) ---
                string dateStr;
                cout << "Date (YYYY-MM-DD) [empty = today]: ";
                getline(cin, dateStr);
                chrono_tp d;
                if (dateStr.empty()) d = today();
                else { if (!tryParseDate(dateStr, d)) { cout << "Invalid date format. Use YYYY-MM-DD.\n"; if (!askReturnToMenu()) break; else continue; } }

                cout << "Amount (positive = income, negative = expense): ";
                double amt;
                if (!(cin >> amt)) {
                    cout << "Invalid amount.\n";
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    if (!askReturnToMenu()) break;
                    else continue;
                }
                cin.ignore(numeric_limits<streamsize>::max(), '\n');

                vector<pair<string,string>> cats; // pair<display, normalized>
                for (auto &p : acc.displayNames) cats.emplace_back(p.second.empty() ? p.first : p.second, p.first);
                sort(cats.begin(), cats.end(), [](auto &a, auto &b){ return a.first < b.first; });

                cout << "Enter category or its number, or leave blank for Auto-allocate (for incomes).\n";
                cout << "Existing categories:\n";
                for (size_t i = 0; i < cats.size(); ++i) {
                    cout << "  " << (i+1) << ". " << cats[i].first << "\n";
                }
                cout << "Category (name or number) [blank = Auto-allocate]: ";
                string catInput;
                getline(cin, catInput);
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
                            } else cout << "Number out of range.\n";
                        } catch (...) {}
                    }
                    if (!handled) {
                        string nk = normalizeKey(catInput);
                        if (acc.displayNames.find(nk) != acc.displayNames.end()) {
                            chosenDisplayCat = acc.displayNames[nk];
                        } else {
                            while (true) {
                                cout << "Category does not exist. Do you want to create \"" << catInput << "\" or retype? (c/r): ";
                                string resp; if (!getline(cin, resp)) resp = "r";
                                trim_inplace(resp);
                                if (!resp.empty() && (resp[0]=='c' || resp[0]=='C')) {
                                    acc.displayNames[nk] = catInput;
                                    if (acc.categoryBalances.find(nk) == acc.categoryBalances.end()) acc.categoryBalances[nk] = 0.0;
                                    chosenDisplayCat = catInput;
                                    break;
                                } else if (!resp.empty() && (resp[0]=='r' || resp[0]=='R')) {
                                    cout << "Retype category (name or number) [blank = Auto-allocate]: ";
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
                                            } else cout << "Number out of range.\n";
                                        } catch (...) {}
                                    }
                                    if (retriedHandled) break;
                                    string nk2 = normalizeKey(catInput);
                                    if (acc.displayNames.find(nk2) != acc.displayNames.end()) {
                                        chosenDisplayCat = acc.displayNames[nk2];
                                        break;
                                    } else {
                                        continue;
                                    }
                                } else {
                                    cout << "Please answer 'c' to create or 'r' to retype.\n";
                                }
                            }
                        }
                    }
                }

                cout << "Note: "; string note; getline(cin, note);

                if (amt > 0.0 && willAutoAllocate) {
                    acc.allocateAmount(d, amt, note + " (manual income)");
                    cout << "Added and auto-allocated by percentages.\n";
                } else {
                    if (chosenDisplayCat.empty()) chosenDisplayCat = "Other";
                    string nkChosen = normalizeKey(chosenDisplayCat);
                    if (acc.displayNames.find(nkChosen) == acc.displayNames.end()) acc.displayNames[nkChosen] = chosenDisplayCat;
                    acc.addManualTransaction(d, amt, chosenDisplayCat, note);
                    cout << "Added.\n";
                }

            } else if (choice == 2) {
                cout << "Type: 1) Every X days  2) Monthly on day D\nChoice: ";
                int t; if (!(cin >> t)) {
                    cout << "Invalid type.\n";
                    cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    if (!askReturnToMenu()) break;
                    else continue;
                }
                cin.ignore(numeric_limits<streamsize>::max(), '\n');

                Schedule s;
                if (t == 1) {
                    s.type = ScheduleType::EveryXDays;
                    cout << "Enter days interval: ";
                    if (!(cin >> s.param)) {
                        cout << "Invalid interval.\n";
                        cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        if (!askReturnToMenu()) break;
                        else continue;
                    }
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    if (s.param <= 0) { cout << "Interval must be > 0.\n"; if (!askReturnToMenu()) break; else continue; }
                } else {
                    s.type = ScheduleType::MonthlyDay;
                    cout << "Enter day of month (1-31): ";
                    if (!(cin >> s.param)) {
                        cout << "Invalid day.\n";
                        cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        if (!askReturnToMenu()) break;
                        else continue;
                    }
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    if (s.param < 1 || s.param > 31) { cout << "Day must be between 1 and 31.\n"; if (!askReturnToMenu()) break; else continue; }
                }

                cout << "Amount (positive for scheduled income, negative for scheduled expense): ";
                if (!(cin >> s.amount)) {
                    cout << "Invalid amount.\n";
                    cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    if (!askReturnToMenu()) break;
                    else continue;
                }
                cin.ignore(numeric_limits<streamsize>::max(), '\n');

                cout << "Note: "; getline(cin, s.note);

                vector<pair<string,string>> cats; // pair<display, normalized>
                for (auto &p : acc.displayNames) cats.emplace_back(p.second.empty() ? p.first : p.second, p.first);
                sort(cats.begin(), cats.end(), [](auto &a, auto &b){ return a.first < b.first; });

                cout << "Enter category or its number, or leave blank for Auto-allocate (for positive amounts).\n";
                cout << "Existing categories:\n";
                for (size_t i = 0; i < cats.size(); ++i) {
                    cout << "  " << (i+1) << ". " << cats[i].first << "\n";
                }
                cout << "Category (name or number) [blank = Auto-allocate when positive]: ";
                string catInput; getline(cin, catInput); trim_inplace(catInput);

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
                                cout << "Number out of range.\n";
                            }
                        } catch (...) {}
                    }
                    if (!handled) {
                        string nk = normalizeKey(catInput);
                        if (acc.displayNames.find(nk) != acc.displayNames.end()) {
                            s.category = acc.displayNames[nk];
                        } else {
                            while (true) {
                                cout << "Category does not exist. Do you want to create \"" << catInput << "\" or retype? (c/r): ";
                                string resp; if (!getline(cin, resp)) resp = "r";
                                trim_inplace(resp);
                                if (!resp.empty() && (resp[0]=='c' || resp[0]=='C')) {
                                    acc.displayNames[nk] = catInput;
                                    if (acc.categoryBalances.find(nk) == acc.categoryBalances.end()) acc.categoryBalances[nk] = 0.0;
                                    s.category = catInput;
                                    break;
                                } else if (!resp.empty() && (resp[0]=='r' || resp[0]=='R')) {
                                    cout << "Retype category (name or number) [blank = Auto-allocate when positive]: ";
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
                                            } else cout << "Number out of range.\n";
                                        } catch (...) {}
                                    }
                                    string nk2 = normalizeKey(catInput);
                                    if (acc.displayNames.find(nk2) != acc.displayNames.end()) {
                                        s.category = acc.displayNames[nk2];
                                        break;
                                    }
                                } else {
                                    cout << "Please answer 'c' to create or 'r' to retype.\n";
                                }
                            }
                        }
                    }
                }

                if (s.autoAllocate && s.amount < 0.0) {
                    cout << "Auto-allocation only applies to positive amounts. This schedule is negative; it will be recorded to a category.\n";
                    if (s.category.empty()) s.category = "Other";
                    s.autoAllocate = false;
                }

                cout << "Start date (YYYY-MM-DD) [today]: ";
                string start; getline(cin, start);
                if (start.empty()) s.nextDate = today();
                else { if (!tryParseDate(start, s.nextDate)) { cout << "Invalid start date.\n"; if (!askReturnToMenu()) break; else continue; } }

                acc.addSchedule(s);
                cout << "Scheduled transaction added.\n";

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
                cout << "Goodbye!\n";
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

        if (!askReturnToMenu()) break;
    }

    return 0;
}
