// Finance Manager v2.1 - Personal Finance Management System
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
// Build: g++ -std=c++17 finance_v2_1.cpp -o finance_v2_1
// Usage: ./finance_v2_1 (interactive mode)

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

struct Schedule {
    ScheduleType type;
    int param; // days interval or day-of-month
    double amount;
    string note;
    bool autoAllocate;
    chrono_tp nextDate;
    string category; // display name (may be empty => use Other or auto-alloc when appropriate)
};

// Interest entry per-category
struct InterestEntry {
    string categoryNormalized; // normalized key
    double ratePct; // stored as percent (e.g., 0.5 means 0.5%)
    bool monthly; // true if monthly rate, false if annual rate
    chrono_tp startDate; // when the rate starts
    chrono_tp lastAppliedDate; // last date interest was applied through (initially set to startDate)
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

// months between inclusive-ish: number of month "periods" from start to end.
// If end < start -> 0
// Calculate: months = (ey - sy)*12 + (em - sm) + (ed >= sd ? 1 : 0)
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

// -------------------- Category normalization & sanitization helpers --------------------
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

// Remove special characters from display name; allow letters, digits, spaces.
// Collapse multiple spaces and trim.
static inline string sanitizeDisplayName(const string &s) {
    string tmp;
    tmp.reserve(s.size());
    for (char c : s) {
        if (isalnum((unsigned char)c) || isspace((unsigned char)c)) tmp.push_back(c);
        // else skip character
    }
    // collapse spaces
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
    // trim
    while (!out.empty() && isspace((unsigned char)out.front())) out.erase(out.begin());
    while (!out.empty() && isspace((unsigned char)out.back())) out.pop_back();
    if (out.empty()) return string("Category");
    return out;
}

// Parse user-entered rate - accepts commas and percent signs.
// Returns rate as double percent (e.g., "0.5%" -> 0.5). Throws if invalid.
static inline bool tryParseRate(const string &sraw, double &outPct) {
    string s = sraw;
    // remove spaces
    s.erase(remove_if(s.begin(), s.end(), [](char c){ return isspace((unsigned char)c); }), s.end());
    // replace comma with dot
    for (char &c : s) if (c == ',') c = '.';
    // remove trailing percent sign if present
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

// -------------------- Account --------------------
struct Account {
    double balance = 0.0;
    vector<Transaction> txs;
    vector<Schedule> schedules;
    map<string, double> allocationPct;      // normalized -> percent
    map<string, double> categoryBalances;   // normalized -> amount
    map<string, string> displayNames;       // normalized -> display name
    map<string, InterestEntry> interestMap; // normalized -> interest entry

    Account() {
        // defaults
        vector<pair<string,double>> defaults = {
            {"Emergency", 20.0}, {"Entertainment", 10.0}, {"Saving", 20.0}, {"Other", 50.0}
        };
        for (auto &p : defaults) {
            string nk = normalizeKey(p.first);
            allocationPct[nk] = p.second;
            displayNames[nk] = p.first;
            categoryBalances[nk] = 0.0;
        }
    }

    void ensureCategoryExists(const string &displayRaw) {
        string display = sanitizeDisplayName(displayRaw);
        string nk = normalizeKey(display);
        if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = display;
        if (categoryBalances.find(nk) == categoryBalances.end()) categoryBalances[nk] = 0.0;
        if (allocationPct.find(nk) == allocationPct.end()) allocationPct[nk] = 0.0;
    }

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

    // Apply interest rates for all categories up to 'upTo' (usually today).
    // For each interest entry, we simulate month-by-month interest using the transaction history
    // so compounding and scheduled transactions are reflected.
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
                chrono_tp applyDate = addMonths(ie.startDate, 
                                                (int)( ( (int)( (safeLocaltime(chrono::system_clock::to_time_t(fromDate)).tm_year+1900) - (safeLocaltime(chrono::system_clock::to_time_t(ie.startDate)).tm_year+1900) ) * 12) 
                                                + (safeLocaltime(chrono::system_clock::to_time_t(fromDate)).tm_mon+1) - (safeLocaltime(chrono::system_clock::to_time_t(ie.startDate)).tm_mon+1) + m));
                // To simplify and be robust, compute applyDate as addMonths(fromDate, m)
                applyDate = addMonths(fromDate, m);

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
        cout << "Interest entries:\n";
        for (auto &kv : interestMap) {
            const InterestEntry &ie = kv.second;
            string display = displayNames.count(ie.categoryNormalized) ? displayNames.at(ie.categoryNormalized) : ie.categoryNormalized;
            cout << "  - " << display << ": " << ie.ratePct << (ie.monthly ? "% monthly" : "% annual (converted monthly)") 
                 << ", start=" << toDateString(ie.startDate) << ", lastApplied=" << toDateString(ie.lastAppliedDate) << "\n";
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
        cout << "Saved to " << filename << "\n";
    }

    // returns true if load succeeded, false if file missing or not readable
    bool loadFromFile(const string &filename = SAVE_FILENAME) {
        ifstream ifs(filename);
        if (!ifs) {
            // do not treat as fatal here, caller will decide to set up or retry
            return false;
        }
        string line;
        enum Section { None, InterestSec, Alloc, Cats, Scheds, Txs } sec = None;
        allocationPct.clear(); categoryBalances.clear(); schedules.clear(); txs.clear(); displayNames.clear(); interestMap.clear();

        double savedBalance = 0.0;
        bool hadSavedBalance = false;

        while (getline(ifs, line)) {
            if (line == "INTERESTS") { sec = InterestSec; continue; }
            if (line == "ALLOCATIONS") { sec = Alloc; continue; }
            if (line == "CATEGORIES") { sec = Cats; continue; }
            if (line == "SCHEDULES") { sec = Scheds; continue; }
            if (line == "TXS") { sec = Txs; continue; }
            if (line.rfind("BALANCE ", 0) == 0) {
                try { savedBalance = stod(line.substr(8)); hadSavedBalance = true; } catch (...) { cerr << "Warning: invalid BALANCE value.\n"; }
            } else {
                if (sec == InterestSec) {
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

        cout << "Loaded from " << filename << "\n";
        return true;
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
    cout << "6) Apply saving interest - set rate(s) and apply interest to selected category(ies).\n";
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
    cout << "6) Apply saving interest\n";
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

// Helper: interactive allocation setup that only uses existing categories (Other is remainder)
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

        cout << "\nEnter percentage for each category, or leave blank to keep current.\n";
        cout << "(Note: 'Other' percent is the remaining after these assignments.)\n";
        cout << "You currently have " << fixed << setprecision(2) << remaining << "% available for Other.\n";

        map<string, double> attempted = newPct; // copy to modify inline
        bool anyChange = false;

        for (auto &k : keys) {
            string display = acc.displayNames[k];
            double cur = 0.0;
            if (attempted.find(k) != attempted.end()) cur = attempted[k];

            // per-category input loop to reject negative or >100 values
            while (true) {
                cout << display << " (current " << cur << "%) - enter new percent or blank to keep: ";
                string line;
                if (!getline(cin, line)) line.clear();
                trim_inplace(line);
                if (line.empty()) break; // keep current
                double val;
                try {
                    size_t pos = 0;
                    val = stod(line, &pos);
                    if (pos != line.size()) { cout << "Invalid input (extra chars). Try again.\n"; continue; }
                } catch (...) {
                    cout << "Invalid input. Try again.\n";
                    continue;
                }
                if (!isfinite(val)) { cout << "Invalid number. Try again.\n"; continue; }
                if (val < 0.0) { cout << "Negative percentages are not allowed. Please enter a value between 0 and 100.\n"; continue; }
                if (val > 100.0) { cout << "Percent cannot exceed 100. Please enter a value between 0 and 100.\n"; continue; }
                attempted[k] = val;
                anyChange = true;
                break;
            }
        }

        // Validate totals
        double total = 0.0;
        for (auto &k : keys) total += attempted[k];
        if (total < -1e-9 || total > 100.0 + 1e-9) {
            cout << "Invalid allocation: categories sum to " << total << "%. Must be between 0 and 100. Please re-enter.\n";
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
        acc.allocationPct[nkOther] = otherPct;
        // Ensure displayName exists for Other
        if (acc.displayNames.find(nkOther) == acc.displayNames.end()) acc.displayNames[nkOther] = "Other";
        cout << "Allocations updated. 'Other' set to remaining " << fixed << setprecision(2) << acc.allocationPct[nkOther] << "%\n";
        break;
    }
}

// Helper: interactive category creation for initial setup
void interactiveCategorySetup(Account &acc) {
    cout << "\n--- Category setup ---\n";
    cout << "Enter category display names, one per line. Press Enter on an empty line to finish.\n";
    cout << "If you enter no categories, defaults will be used.\n";
    vector<string> newCats;
    while (true) {
        cout << "Category name (empty = finish): ";
        string line;
        if (!getline(cin, line)) line.clear();
        trim_inplace(line);
        if (line.empty()) break;
        string sanitized = sanitizeDisplayName(line);
        newCats.push_back(sanitized);
    }
    if (newCats.empty()) {
        cout << "No categories entered. Using defaults (Emergency, Entertainment, Saving, Other).\n";
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
    cout << "Categories created.\n";
}

// Called from main when user chooses to set up new account
void runInitialSetup(Account &acc) {
    interactiveCategorySetup(acc);
    cout << "\nWould you like to set allocation percentages now? (s = set now, l = leave and set later) [s/l]: ";
    string resp;
    if (!getline(cin, resp)) resp = "l";
    trim_inplace(resp);
    if (!resp.empty() && (resp[0]=='s' || resp[0]=='S')) {
        interactiveAllocSetup(acc, true);
    } else {
        cout << "You can set allocations later from menu option 4.\n";
    }
    cout << "You are all set!\n";
}

// -------------------- Main --------------------
int main() {
    #include <filesystem>
    std::cout << "Working directory: " << std::filesystem::current_path() << '\n';

    ios::sync_with_stdio(false);
    cin.tie(&cout);
    Account acc;

    // Attempt to load; if missing, ask user whether to set up or retry
    bool loaded = acc.loadFromFile();
    if (!loaded) {
        cout << "Cannot open file to load. No existing save found in the working directory.\n";
        while (true) {
            cout << "Choose: (s)et up new account, (r)etry loading save file after placing it in working directory: ";
            string resp;
            if (!getline(cin, resp)) { resp = "s"; }
            trim_inplace(resp);
            if (!resp.empty() && (resp[0]=='s' || resp[0]=='S')) {
                runInitialSetup(acc);
                break;
            } else if (!resp.empty() && (resp[0]=='r' || resp[0]=='R')) {
                cout << "Retrying load...\n";
                if (acc.loadFromFile()) {
                    break;
                } else {
                    cout << "Still cannot find save file. You can place 'finance_save.txt' into the working directory and choose (r) again, or choose (s) to set up new.\n";
                    continue;
                }
            } else {
                cout << "Please answer 's' to set up or 'r' to retry.\n";
            }
        }
    }

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
                        string sanitized = sanitizeDisplayName(catInput);
                        string nk = normalizeKey(sanitized);
                        if (acc.displayNames.find(nk) != acc.displayNames.end()) {
                            chosenDisplayCat = acc.displayNames[nk];
                        } else {
                            while (true) {
                                cout << "Category does not exist. Do you want to create \"" << sanitized << "\" or retype? (c/r): ";
                                string resp; if (!getline(cin, resp)) resp = "r";
                                trim_inplace(resp);
                                if (!resp.empty() && (resp[0]=='c' || resp[0]=='C')) {
                                    acc.displayNames[nk] = sanitized;
                                    if (acc.categoryBalances.find(nk) == acc.categoryBalances.end()) acc.categoryBalances[nk] = 0.0;
                                    if (acc.allocationPct.find(nk) == acc.allocationPct.end()) acc.allocationPct[nk] = 0.0;
                                    chosenDisplayCat = sanitized;
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
                                    string sanitized2 = sanitizeDisplayName(catInput);
                                    string nk2 = normalizeKey(sanitized2);
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
                    string nkChosen = normalizeKey(sanitizeDisplayName(chosenDisplayCat));
                    if (acc.displayNames.find(nkChosen) == acc.displayNames.end()) acc.displayNames[nkChosen] = sanitizeDisplayName(chosenDisplayCat);
                    acc.addManualTransaction(d, amt, acc.displayNames[nkChosen], note);
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
                        string sanitized = sanitizeDisplayName(catInput);
                        string nk = normalizeKey(sanitized);
                        if (acc.displayNames.find(nk) != acc.displayNames.end()) {
                            s.category = acc.displayNames[nk];
                        } else {
                            while (true) {
                                cout << "Category does not exist. Do you want to create \"" << sanitized << "\" or retype? (c/r): ";
                                string resp; if (!getline(cin, resp)) resp = "r";
                                trim_inplace(resp);
                                if (!resp.empty() && (resp[0]=='c' || resp[0]=='C')) {
                                    acc.displayNames[nk] = sanitized;
                                    if (acc.categoryBalances.find(nk) == acc.categoryBalances.end()) acc.categoryBalances[nk] = 0.0;
                                    if (acc.allocationPct.find(nk) == acc.allocationPct.end()) acc.allocationPct[nk] = 0.0;
                                    s.category = sanitized;
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
                                    string sanitized2 = sanitizeDisplayName(catInput);
                                    string nk2 = normalizeKey(sanitized2);
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
                // New allocation setup: operate only on existing categories (Other is remainder)
                interactiveAllocSetup(acc, false);

            } else if (choice == 5) {
                acc.processSchedulesUpTo(today());
                cout << "Processed schedules.\n";

            } else if (choice == 6) {
                // New flow: let user choose to (A)dd/Update interest entry, (R)emove, or (P)lay now apply interest up to today
                cout << "Interest menu: (a)dd/update rate, (r)emove rate, (p)rocess/apply interest now: ";
                string sub; if (!getline(cin, sub)) sub = "p";
                trim_inplace(sub);
                if (!sub.empty() && (sub[0]=='a' || sub[0]=='A')) {
                    // Add or update interest entries for one or more categories
                    // List categories:
                    vector<pair<string,string>> cats; // display, normalized
                    for (auto &p : acc.displayNames) cats.emplace_back(p.second.empty() ? p.first : p.second, p.first);
                    sort(cats.begin(), cats.end(), [](auto &a, auto &b){ return a.first < b.first; });
                    cout << "Existing categories:\n";
                    for (size_t i = 0; i < cats.size(); ++i) {
                        cout << "  " << (i+1) << ". " << cats[i].first << "\n";
                    }
                    cout << "Please choose which category(/ies) to apply interest rate (number(s) separated by comma, or names separated by comma): ";
                    string catSel; if (!getline(cin, catSel)) catSel.clear();
                    trim_inplace(catSel);
                    if (catSel.empty()) { cout << "No categories selected.\n"; if (!askReturnToMenu()) break; else continue; }
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
                                    cout << "Number out of range for selection: " << s << "\n";
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
                                cout << "Category not found: " << s << " (sanitized -> " << sanitized << "). Skipping.\n";
                                continue;
                            }
                        }
                    }
                    if (targets.empty()) { cout << "No valid categories selected.\n"; if (!askReturnToMenu()) break; else continue; }

                    // Ask monthly or annual
                    cout << "(M)onthly or (A)nnually? [m/a]: ";
                    string ma; if (!getline(cin, ma)) ma = "m";
                    trim_inplace(ma);
                    bool monthly = true;
                    if (!ma.empty() && (ma[0]=='a' || ma[0]=='A')) monthly = false;
                    // Ask for rate value
                    cout << "Enter rate (numbers accepted like '0.5%', '0,5', '5.5', '5,5%'): ";
                    string rateIn; if (!getline(cin, rateIn)) rateIn.clear();
                    trim_inplace(rateIn);
                    double ratePct = 0.0;
                    if (!tryParseRate(rateIn, ratePct)) {
                        cout << "Invalid rate input.\n"; if (!askReturnToMenu()) break; else continue;
                    }
                    // Ask for start date
                    cout << "The interest rate is applied since (YYYY-MM-DD) [leave blank = today]: ";
                    string startIn; if (!getline(cin, startIn)) startIn.clear();
                    trim_inplace(startIn);
                    chrono_tp startDate = today();
                    if (!startIn.empty()) {
                        if (!tryParseDate(startIn, startDate)) {
                            cout << "Invalid start date.\n"; if (!askReturnToMenu()) break; else continue;
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
                        cout << "Set " << (monthly ? "monthly" : "annual") << " rate " << ratePct << "% for category '" << acc.displayNames[nk] << "' starting " << toDateString(startDate) << ".\n";
                    }
                } else if (!sub.empty() && (sub[0]=='r' || sub[0]=='R')) {
                    // remove interest entries
                    if (acc.interestMap.empty()) { cout << "No interest entries to remove.\n"; if (!askReturnToMenu()) break; else continue; }
                    cout << "Interest entries:\n";
                    vector<pair<int,string>> idxToNk;
                    int i = 1;
                    for (auto &kv : acc.interestMap) {
                        string disp = acc.displayNames.count(kv.first) ? acc.displayNames[kv.first] : kv.first;
                        cout << "  " << i << ") " << disp << " : " << kv.second.ratePct << (kv.second.monthly ? "% monthly" : "% annual") << "\n";
                        idxToNk.push_back({i, kv.first});
                        ++i;
                    }
                    cout << "Enter number(s) to remove (comma separated): ";
                    string rem; if (!getline(cin, rem)) rem.clear();
                    trim_inplace(rem);
                    if (rem.empty()) { cout << "No selection made.\n"; if (!askReturnToMenu()) break; else continue; }
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
                                cout << "Removed interest for " << (acc.displayNames.count(p.second) ? acc.displayNames[p.second] : p.second) << "\n";
                            }
                        } catch (...) {}
                    }
                } else {
                    // process / apply interest now
                    cout << "Processing & applying interest up to today (" << toDateString(today()) << ")...\n";
                    acc.applyInterestUpTo(today());
                    cout << "Interest applied.\n";
                }

            } else if (choice == 7) {
                acc.saveToFile();

            } else if (choice == 8) {
                bool ok = acc.loadFromFile();
                if (!ok) {
                    cout << "Cannot open file to load. Place 'finance_save.txt' in the working directory and try again.\n";
                }

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
