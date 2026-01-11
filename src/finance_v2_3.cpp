// Finance Manager v2.3 - Personal Finance Management System
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
// Build: g++ -std=c++17 finance_v2_3.cpp -o finance_v2_3
// Usage: ./finance_v2_3 (interactive mode)

#include <bits/stdc++.h>
using namespace std;
using chrono_tp = chrono::system_clock::time_point;

static const string SAVE_FILENAME = "finance_save.txt";

// -------------------- Types --------------------
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

// START REPLACE: InterestEntry - switch semantics to nextApplyDate (replace whole struct block)
// Interest entry per-category
struct InterestEntry {
    string categoryNormalized; // normalized key
    double ratePct; // stored as percent (e.g., 0.5 means 0.5%)
    bool monthly; // true if monthly rate, false if annual rate
    chrono_tp startDate; // when the rate starts

    // IMPORTANT: nextApplyDate is the date of the *next* application of interest.
    // Example: if interest was applied through 2024-01-01, nextApplyDate == 2024-02-01
    // This removes ambiguity of "last applied inclusive" vs "next to apply".
    chrono_tp nextApplyDate; // next date at which interest should be applied

    InterestEntry() : ratePct(0.0), monthly(false) {}
};
// END REPLACE: InterestEntry

// Settings container
struct Settings {
    bool autoSave = false;
    bool autoProcessOnStartup = false;
    string language = "EN"; // "EN" or "VI"
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

// START REPLACE: monthsBetweenInclusive - safer count by iterating addMonths
static inline int monthsBetweenInclusive(const chrono_tp &start, const chrono_tp &end) {
    // Count how many monthly "apply points" exist from 'start' (inclusive) while advancing by whole months,
    // where each step is defined by addMonths(previous,1). This avoids off-by-one around end-of-month and DST.
    time_t stt = chrono::system_clock::to_time_t(start);
    time_t ett = chrono::system_clock::to_time_t(end);
    if (ett < stt) return 0;

    int count = 0;
    chrono_tp cur = start;
    // If start's date-of-month is after the 'end' in the first month, we still consider whether the first apply
    // should happen on the same month depending on date equality. We'll treat 'cur' as the first apply point.
    while (true) {
        time_t curt = chrono::system_clock::to_time_t(cur);
        if (curt > ett) break;
        ++count;
        chrono_tp next = addMonths(cur, 1);
        // safety: if next == cur (shouldn't happen) break to avoid infinite loop
        time_t nextt = chrono::system_clock::to_time_t(next);
        if (nextt <= curt) break;
        cur = next;
    }
    return count;
}
// END REPLACE: monthsBetweenInclusive

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

// START REPLACE/INSERT: addOrMergeTransaction + logging helpers + LOG_FILENAME
// ---- forward declaration ----
static inline string normalizeKey(const string &s);

static const string LOG_FILENAME = "finance_full_log.txt";

// Append a line to the full append-only log. Timestamp + message.
static inline void logChange(const string &msg) {
    ofstream ofs(LOG_FILENAME, ios::app);
    if (!ofs) return;
    auto now = chrono::system_clock::now();
    time_t tt = chrono::system_clock::to_time_t(now);
    tm t = safeLocaltime(tt);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    ofs << "[" << buf << "] " << msg << "\n";
    ofs.close();
}

// Merge semantics: if existing transaction with same date+category+note exists, sum amounts.
// Otherwise push new transaction. This reduces transaction explosion and helps batching.
static inline void addOrMergeTransaction(vector<Transaction> &txsRef,
                                        const chrono_tp &date,
                                        double amount,
                                        const string &category,
                                        const string &note,
                                        double &accountBalance,
                                        map<string,double> &categoryBalances,
                                        bool doMerge = true) {
    if (!doMerge) {
        txsRef.push_back({date, amount, category, note});
        accountBalance += amount;
        string nk = normalizeKey(category);
        categoryBalances[nk] += amount;
        return;
    }
    // find existing tx with same date+category+note
    for (auto &t : txsRef) {
        if (t.date == date && t.category == category && t.note == note) {
            t.amount += amount;
            accountBalance += amount;
            string nk = normalizeKey(category);
            categoryBalances[nk] += amount;
            return;
        }
    }
    txsRef.push_back({date, amount, category, note});
    accountBalance += amount;
    string nk = normalizeKey(category);
    categoryBalances[nk] += amount;
}
// END REPLACE/INSERT: addOrMergeTransaction + logging helpers

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

// START REPLACE: tryParseRate robust
static inline bool tryParseRate(const string &sraw, double &outPct) {
    string s = sraw;
    // remove spaces
    s.erase(remove_if(s.begin(), s.end(), [](char c){ return isspace((unsigned char)c); }), s.end());
    // replace comma with dot
    for (char &c : s) if (c == ',') c = '.';
    // remove trailing percent sign if present
    if (!s.empty() && s.back() == '%') { s.pop_back(); }
    if (s.empty()) return false;
    try {
        size_t pos = 0;
        double v = stod(s, &pos);
        if (pos != s.size()) return false;
        if (!isfinite(v)) return false;
        outPct = v;
        return true;
    } catch (const std::invalid_argument &) {
        return false;
    } catch (const std::out_of_range &) {
        return false;
    } catch (...) {
        return false;
    }
}
// END REPLACE: tryParseRate

// -------------------- Account --------------------
struct Account {
    double balance = 0.0;
    vector<Transaction> txs;
    vector<Schedule> schedules;
    map<string, double> allocationPct;      // normalized -> percent
    map<string, double> categoryBalances;   // normalized -> amount
    map<string, string> displayNames;       // normalized -> display name
    map<string, InterestEntry> interestMap; // normalized -> interest entry
    Settings settings;

    // START REPLACE/INSERT: Snapshot helpers inside Account
    struct Snapshot {
        size_t txCount;
        double balance;
        map<string,double> categoryBalances;
        map<string, InterestEntry> interestMap;
        vector<Schedule> schedules;
    };

    Snapshot createSnapshot() const {
        Snapshot s;
        s.txCount = txs.size();
        s.balance = balance;
        s.categoryBalances = categoryBalances;
        s.interestMap = interestMap;
        s.schedules = schedules;
        return s;
    }

    void restoreSnapshot(const Snapshot &s) {
        // Remove transactions added after snapshot
        if (txs.size() > s.txCount) txs.resize(s.txCount);
        balance = s.balance;
        categoryBalances = s.categoryBalances;
        interestMap = s.interestMap;
        schedules = s.schedules;
        logChange("Restored snapshot (undo) to txCount=" + to_string(s.txCount));
    }
    // END REPLACE/INSERT: Snapshot helpers

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
        // settings defaults already set by Settings ctor
    }

    void ensureCategoryExists(const string &displayRaw) {
        string display = sanitizeDisplayName(displayRaw);
        string nk = normalizeKey(display);
        if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = display;
        if (categoryBalances.find(nk) == categoryBalances.end()) categoryBalances[nk] = 0.0;
        if (allocationPct.find(nk) == allocationPct.end()) allocationPct[nk] = 0.0;
    }

    // Reworked: use addOrMergeTransaction so we don't produce duplicate txs for same date/category/note
    void addManualTransaction(const chrono_tp &date, double amount, const string &categoryRaw, const string &note) {
        string catDisplay = categoryRaw.empty() ? "Other" : sanitizeDisplayName(categoryRaw);
        string nk = normalizeKey(catDisplay);
        if (displayNames.find(nk) == displayNames.end()) displayNames[nk] = catDisplay;
        // use addOrMergeTransaction to merge same date/category/note
        addOrMergeTransaction(txs, date, amount, displayNames[nk], note, balance, categoryBalances, true);
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
            // merged push fallback
            addOrMergeTransaction(txs, date, amount, displayNames[nk], note + " (auto alloc fallback)", balance, categoryBalances, true);
            return;
        }
        for (auto &p : allocationPct) {
            double share = amount * (p.second / totalPct);
            categoryBalances[p.first] += share;
            string catDisplay = displayNames[p.first].empty() ? p.first : displayNames[p.first];
            addOrMergeTransaction(txs, date, share, catDisplay, note + " (auto alloc)", balance, categoryBalances, true);
        }
    }

    void addSchedule(const Schedule &s) {
        schedules.push_back(s);
    }

    // START REPLACE: processSchedulesUpTo - safe iteration, dry-run/preview, logging, merge tx
    // Note: signature extends with optional dryRun + previewOut but keeps default so existing calls work.
    void processSchedulesUpTo(const chrono_tp &upTo, bool dryRun = false, vector<Transaction>* previewOut = nullptr) {
        for (auto &s : schedules) {
            if (s.type == ScheduleType::EveryXDays && s.param <= 0) {
                string msg = "Skipping schedule with non-positive interval (EveryXDays param=" + to_string(s.param) + ")";
                cerr << msg << "\n";
                logChange(msg);
                continue;
            }
            if (s.type == ScheduleType::MonthlyDay && (s.param < 1 || s.param > 31)) {
                string msg = "Skipping schedule with invalid day-of-month (param=" + to_string(s.param) + ")";
                cerr << msg << "\n";
                logChange(msg);
                continue;
            }

            // Compute a conservative max iterations to avoid infinite loops:
            // - For EveryXDays: days span / param + 5
            // - For monthly: monthsBetweenInclusive(nextDate, upTo) + 5
            int maxIter = 1000; // fallback
            if (s.type == ScheduleType::EveryXDays) {
                // days span
                time_t st = chrono::system_clock::to_time_t(s.nextDate);
                time_t en = chrono::system_clock::to_time_t(upTo);
                int daysSpan = (int)max(0LL, (long long)((en - st) / (60*60*24)));
                maxIter = (s.param > 0) ? (daysSpan / max(1, s.param) + 5) : 0;
                maxIter = min(maxIter, 100000); // hard cap
            } else {
                int monthsSpan = monthsBetweenInclusive(s.nextDate, upTo);
                maxIter = monthsSpan + 5;
                maxIter = min(maxIter, 1200);
            }

            int iter = 0;
            while (s.nextDate <= upTo && iter < maxIter) {
                // Build what would be the transaction (but don't commit if dryRun)
                string note = string("Scheduled: ") + s.note;
                if (s.autoAllocate && s.amount > 0.0) {
                    if (dryRun) {
                        if (previewOut) {
                            // simulate allocations (we won't split here; preview will show total amount and note)
                            previewOut->push_back({s.nextDate, s.amount, string("<<auto-alloc>>"), note});
                        }
                    } else {
                        // allocateAmount will create multiple txs which now use addOrMergeTransaction internally
                        allocateAmount(s.nextDate, s.amount, note);
                        // log
                        logChange("Applied scheduled auto-alloc: date=" + toDateString(s.nextDate) + " amount=" + to_string(s.amount) + " note=" + s.note);
                    }
                } else {
                    string cat = s.category.empty() ? string("Other") : s.category;
                    if (dryRun) {
                        if (previewOut) previewOut->push_back({s.nextDate, s.amount, cat, note});
                    } else {
                        // Use addOrMergeTransaction to avoid explosion
                        addOrMergeTransaction(txs, s.nextDate, s.amount, cat, note, balance, categoryBalances, true);
                        logChange("Applied scheduled tx: date=" + toDateString(s.nextDate) + " amount=" + to_string(s.amount) + " category=" + cat + " note=" + s.note);
                    }
                }

                // advance
                if (s.type == ScheduleType::EveryXDays) s.nextDate = addDays(s.nextDate, s.param);
                else s.nextDate = nextMonthlyOn(s.nextDate, s.param);

                ++iter;
            }

            if (iter >= maxIter) {
                string msg = "Warning: schedule processing reached max iterations for a schedule (iter=" + to_string(iter) + "). Skipping further iterations for safety.";
                cerr << msg << "\n";
                logChange(msg);
            }
        }

        if (!dryRun) {
            // Optionally sort txs by date for readability (not required)
            sort(txs.begin(), txs.end(), [](const Transaction &a, const Transaction &b){ return a.date < b.date; });
            logChange("Finished processing schedules up to " + toDateString(upTo));
        }
    }
    // END REPLACE: processSchedulesUpTo

    // START REPLACE: applyInterestUpTo - use nextApplyDate semantics, dry-run, merge, logging, snapshot
    // Extended signature: default dryRun=false keeps existing calls compatible.
    void applyInterestUpTo(const chrono_tp &upTo, bool dryRun = false, vector<Transaction>* previewOut = nullptr) {
        if (interestMap.empty()) return;

        // workingTxs used only for simulation of balances when computing interest
        // but we will not mutate account state when dryRun==true
        vector<Transaction> workingTxs = txs;

        for (auto &kv : interestMap) {
            InterestEntry &ie = kv.second;
            // skip if startDate > upTo
            if (ie.startDate > upTo) continue;

            // Ensure nextApplyDate is at least startDate
            if (ie.nextApplyDate < ie.startDate) ie.nextApplyDate = ie.startDate;

            // Compute number of months to apply by iterating from nextApplyDate while <= upTo
            int months = monthsBetweenInclusive(ie.nextApplyDate, upTo);
            if (months <= 0) continue;

            // monthly rate: if entry is monthly then it's already monthly; otherwise annual/12
            double monthlyRate = ie.monthly ? (ie.ratePct / 100.0) : ((ie.ratePct / 100.0) / 12.0);

            chrono_tp applyBase = ie.nextApplyDate;
            for (int m = 0; m < months; ++m) {
                chrono_tp applyDate = addMonths(applyBase, m);

                // compute balance for this category up to applyDate inclusive
                double bal = 0.0;
                string nk = ie.categoryNormalized;
                for (auto &t : workingTxs) {
                    if (normalizeKey(t.category) != nk) continue;
                    if (t.date <= applyDate) bal += t.amount;
                }

                if (bal <= 0.0) {
                    // nothing to apply, but still advance nextApplyDate below
                    if (!dryRun) {
                        logChange("No positive balance for interest on " + (displayNames.count(nk)?displayNames[nk]:nk) + " at " + toDateString(applyDate) + " (bal=" + to_string(bal) + ")");
                    }
                } else {
                    double interest = bal * monthlyRate;
                    if (fabs(interest) > 1e-12) {
                        string display = displayNames.count(nk) ? displayNames[nk] : nk;
                        string note = string("Interest (") + (ie.monthly ? "monthly" : "annual/converted monthly") + ")";
                        if (dryRun) {
                            if (previewOut) previewOut->push_back({applyDate, interest, display, note});
                        } else {
                            // Use addOrMergeTransaction to avoid tx explosion
                            addOrMergeTransaction(txs, applyDate, interest, display, note, balance, categoryBalances, true);
                            workingTxs.push_back({applyDate, interest, display, note}); // so compounding affects following months
                            logChange("Applied interest for " + display + " date=" + toDateString(applyDate) + " interest=" + to_string(interest) + " baseBal=" + to_string(bal));
                        }
                    }
                }
            }

            // Advance nextApplyDate by 'months' months (commit only when not dryRun)
            chrono_tp newNext = addMonths(ie.nextApplyDate, months);
            if (!dryRun) {
                ie.nextApplyDate = newNext;
                // ensure not beyond upTo
                if (ie.nextApplyDate > upTo) ie.nextApplyDate = upTo;
                logChange("Interest nextApplyDate for " + ie.categoryNormalized + " advanced to " + toDateString(ie.nextApplyDate));
            }
        }

        if (!dryRun) {
            // Keep txs sorted for presentation
            sort(txs.begin(), txs.end(), [](const Transaction &a, const Transaction &b){ return a.date < b.date; });
            logChange("Finished applying interest up to " + toDateString(upTo));
        }
    }
    // END REPLACE: applyInterestUpTo

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
            // show nextApplyDate explicitly
            cout << "  - " << display << ": " << ie.ratePct << (ie.monthly ? "% monthly" : "% annual (converted monthly)") 
                 << ", start=" << toDateString(ie.startDate) << ", nextApply=" << toDateString(ie.nextApplyDate) << "\n";
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
        cout << "\nFull operation log: " + LOG_FILENAME + "\n";
        std::cout << "Working directory: " << std::filesystem::current_path() << '\n';
    }

    // START REPLACE: saveToFile atomic tmp+rename, and also write a human-readable snapshot to log
    void saveToFile(const string &filename = SAVE_FILENAME) {
        // write to temporary file first
        string tmpname = filename + ".tmp";
        ofstream ofs(tmpname, ios::trunc);
        if (!ofs) { cerr << "Cannot open temp file to save.\n"; return; }
        ofs << fixed << setprecision(10);
        ofs << "BALANCE " << balance << "\n";
        // SETTINGS
        ofs << "SETTINGS\n";
        ofs << "AUTO_SAVE|" << (settings.autoSave ? "1" : "0") << "\n";
        ofs << "AUTO_PROCESS_STARTUP|" << (settings.autoProcessOnStartup ? "1" : "0") << "\n";
        ofs << "LANGUAGE|" << settings.language << "\n";

        ofs << "INTERESTS\n";
        // Save: category|rate|monthly|start|nextApply (we store nextApplyDate as lastAppliedDate-compat by saving previous apply point)
        for (auto &kv : interestMap) {
            auto &ie = kv.second;
            string display = displayNames.count(ie.categoryNormalized) ? displayNames[ie.categoryNormalized] : ie.categoryNormalized;
            // For backward compatibility with older readers expecting lastApplied, compute a "lastApplied" as addMonths(nextApplyDate, -1)
            chrono_tp lastApplied = addMonths(ie.nextApplyDate, -1);
            ofs << escapeForSave(display) << "|" << ie.ratePct << "|" << (ie.monthly ? "1" : "0")
                << "|" << escapeForSave(toDateString(ie.startDate)) << "|" << escapeForSave(toDateString(lastApplied)) << "\n";
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

        // Atomically move into place
        try {
            #include <filesystem>
            std::filesystem::rename(tmpname, filename);
        } catch (...) {
            // On some platforms, rename over existing file may fail; try remove+rename
            remove(filename.c_str());
            if (std::rename(tmpname.c_str(), filename.c_str()) != 0) {
                cerr << "Atomic rename failed; attempted fallback also failed. Save may be inconsistent.\n";
                logChange(string("ERROR: atomic save failed for ") + filename);
                return;
            }
        }
        logChange(string("Saved data file: ") + filename);
        // UI message
        if (settings.language == "VI") cout << "Đã lưu vào " << filename << "\n";
        else cout << "Saved to " << filename << "\n";
    }
    // END REPLACE: saveToFile

    // returns true if load succeeded, false if file missing or not readable
    bool loadFromFile(const string &filename = SAVE_FILENAME) {
        ifstream ifs(filename);
        if (!ifs) {
            // do not treat as fatal here, caller will decide to set up or retry
            return false;
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
                        // Convert stored 'lastApplied' (legacy) into nextApplyDate semantics:
                        // If lastd < startd, start nextApplyDate at startd. Otherwise nextApplyDate = addMonths(lastd,1).
                        if (lastd < startd) ie.nextApplyDate = startd;
                        else ie.nextApplyDate = addMonths(lastd, 1);
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

// -------------------- Translation helper --------------------
static inline string tr(const Settings &s, const string &id) {
    // Minimal set of translations used in UI. Expand as needed.
    bool vi = (s.language == "VI");
    if (id == "menu_title") return vi ? "=== Trình quản lý Tài chính ===" : "=== Finance Manager ===";
    if (id == "menu_H") return vi ? "H) Hướng dẫn bắt đầu" : "H) Starting Guide";
    if (id == "menu_1") return vi ? "1) Thêm giao dịch thủ công" : "1) Add manual transaction";
    if (id == "menu_2") return vi ? "2) Thêm giao dịch định kỳ" : "2) Add scheduled transaction";
    if (id == "menu_3") return vi ? "3) Hiển thị tóm tắt" : "3) Show summary";
    if (id == "menu_4") return vi ? "4) Thiết lập tỉ lệ phân bổ" : "4) Set allocation percentages";
    if (id == "menu_5") return vi ? "5) Xử lý lịch đến hôm nay" : "5) Process schedules up to today";
    if (id == "menu_6") return vi ? "6) Áp dụng lãi tiết kiệm" : "6) Apply saving interest";
    if (id == "menu_7") return vi ? "7) Lưu" : "7) Save";
    if (id == "menu_8") return vi ? "8) Tải" : "8) Load";
    if (id == "menu_9") return vi ? "9) Thoát" : "9) Exit";
    if (id == "menu_10") return vi ? "10) Cài đặt" : "10) Settings";
    if (id == "choice") return vi ? "Lựa chọn: " : "Choice: ";

    if (id == "starting_guide_title") return vi ? "\n=== Hướng dẫn bắt đầu ===\n" : "\n=== Starting Guide ===\n";
    if (id == "guide_H") return vi ? "H) Hướng dẫn bắt đầu - hiện hướng dẫn này.\n" : "H) Starting Guide - show this help message.\n";
    if (id == "guide_1") return vi ? "1) Thêm giao dịch thủ công - ngày YYYY-MM-DD (trống = hôm nay), số tiền (+ thu nhập / - chi tiêu), loại, ghi chú.\n" : "1) Add manual transaction - date YYYY-MM-DD (empty = today), amount (+ income / - expense), category, note.\n";
    if (id == "guide_2") return vi ? "2) Thêm giao dịch định kỳ - lặp mỗi X ngày hoặc hàng tháng vào ngày D.\n" : "2) Add scheduled transaction - recurring every X days or monthly on day D.\n";
    if (id == "guide_3") return vi ? "3) Hiển thị tóm tắt - tổng số dư, số dư theo loại, phân bổ, giao dịch gần đây.\n" : "3) Show summary - total balance, category balances, allocations, recent txs.\n";
    if (id == "guide_4") return vi ? "4) Thiết lập tỉ lệ phân bổ - định nghĩa cách thu nhập được chia.\n" : "4) Set allocation percentages - define how income is split across categories.\n";
    if (id == "guide_5") return vi ? "5) Xử lý lịch đến hôm nay - áp dụng các giao dịch định kỳ đến hôm nay.\n" : "5) Process schedules up to today - apply scheduled transactions that are due.\n";
    if (id == "guide_6") return vi ? "6) Áp dụng lãi tiết kiệm - đặt lãi cho các loại và áp dụng.\n" : "6) Apply saving interest - set rate(s) and apply interest to selected category(ies).\n";
    if (id == "guide_7") return vi ? ("7) Lưu - ghi dữ liệu hiện tại vào " + SAVE_FILENAME + ".\n") : ("7) Save - write current data to " + SAVE_FILENAME + ".\n");
    if (id == "guide_8") return vi ? ("8) Tải - tải dữ liệu từ " + SAVE_FILENAME + ".\n") : ("8) Load - load data from " + SAVE_FILENAME + ".\n");
    if (id == "guide_9") return vi ? "9) Thoát - thoát chương trình.\n" : "9) Exit - quit program.\n";
    if (id == "guide_10") return vi ? "10) Cài đặt - mở Cài đặt (Tự động lưu, Tự động xử lý khi khởi động, Ngôn ngữ, Nuke).\n" : "10) Settings - open Settings (Auto-save, Auto-process at startup, Language, Nuke).\n";
    if (id == "guide_return") return vi ? "- Hành vi trả về:\n* Nhấn Enter để quay lại menu.\n* Gõ 's' để lưu và thoát chương trình.\n" : "- Return behavior:\n- After each action you'll be prompted: 'Enter to return to Main Interface or (s)ave and exist:'\n  * Press Enter to return to menu.\n  * Enter 's' to save and exit the program.\n";
    if (id == "press_enter") return vi ? "Nhấn Enter để quay lại.\n" : "Press Enter to go back.\n";

    if (id == "saved_exit_prompt") return vi ? "\nNhấn Enter để quay lại giao diện chính hoặc (s) lưu và thoát: " : "\nEnter to return to Main Interface or (s)ave and exist: ";
    if (id == "invalid_choice") return vi ? "Lựa chọn không hợp lệ.\n" : "Invalid choice.\n";
    if (id == "goodbye") return vi ? "Tạm biệt!\n" : "Goodbye!\n";
    if (id == "cannot_open_load") return vi ? ("Không thể mở tệp để tải. Đặt '" + SAVE_FILENAME + "' vào thư mục làm việc và thử lại.\n") : ("Cannot open file to load. Place '" + SAVE_FILENAME + "' in the working directory and try again.\n");
    if (id == "nuke_confirm") return vi ? "Bạn có chắc muốn xóa mọi thứ không? (Mọi dữ liệu sẽ trở về trạng thái ban đầu) [y/N]: " : "Do you really want to nuke your program? (Everything will return to its initial state) [y/N]: ";
    if (id == "nuke_done") return vi ? "Chương trình đã được nuke: trả về trạng thái ban đầu.\n" : "Program nuked: returned to initial state.\n";
    if (id == "nuke_cancel") return vi ? "Hủy nuke.\n" : "Nuke cancelled.\n";
    if (id == "no_save_file") return vi ? "Không có tệp lưu để xóa hoặc xóa thất bại (có thể không tồn tại).\n" : "No save file to remove or failed to remove (it might not exist).\n";

    // Fallback to English raw id
    return id;
}

// -------------------- UI --------------------
void printStartingGuide(const Settings &s) {
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

void printMenu(const Settings &s) {
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

static inline void  trim_inplace(string &s) {
    while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
}

// New return prompt: Enter to return, or (s)ave and exit
// Returns true to continue the main loop, false to exit program
static inline bool askReturnToMenuOrSave(Account &acc) {
    cout << tr(acc.settings, "saved_exit_prompt");
    string resp;
    if (!getline(cin, resp)) return false;
    trim_inplace(resp);
    if (resp.empty()) return true; // just return to menu
    if (resp[0] == 's' || resp[0] == 'S') {
        acc.saveToFile();
        if (acc.settings.language == "VI") cout << "Đã lưu. Thoát.\n";
        else cout << "Saved. Exiting.\n";
        return false;
    }
    // For any other input, treat as return to menu (less likely to accidentally exit)
    return true;
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

        if (acc.settings.language == "VI") {
            cout << "\nNhập phần trăm cho mỗi loại, để trống để giữ giá trị hiện tại.\n";
            cout << "(Ghi chú: 'Other' là phần còn lại sau khi gán.)\n";
            cout << "Bạn hiện có " << fixed << setprecision(2) << remaining << "% dành cho Other.\n";
        } else {
            cout << "\nEnter percentage for each category, or leave blank to keep current.\n";
            cout << "(Note: 'Other' percent is the remaining after these assignments.)\n";
            cout << "You currently have " << fixed << setprecision(2) << remaining << "% available for Other.\n";
        }

        map<string, double> attempted = newPct; // copy to modify inline
        bool anyChange = false;

        for (auto &k : keys) {
            string display = acc.displayNames[k];
            double cur = 0.0;
            if (attempted.find(k) != attempted.end()) cur = attempted[k];

            // per-category input loop to reject negative or >100 values
            while (true) {
                if (acc.settings.language == "VI")
                    cout << display << " (hiện " << cur << "%) - nhập phần trăm mới hoặc trống để giữ: ";
                else
                    cout << display << " (current " << cur << "%) - enter new percent or blank to keep: ";
                string line;
                if (!getline(cin, line)) line.clear();
                trim_inplace(line);
                if (line.empty()) break; // keep current
                double val;
                try {
                    size_t pos = 0;
                    val = stod(line, &pos);
                    if (pos != line.size()) { if (acc.settings.language == "VI") cout << "Dữ liệu không hợp lệ (ký tự thừa). Thử lại.\n"; else cout << "Invalid input (extra chars). Try again.\n"; continue; }
                } catch (...) {
                    if (acc.settings.language == "VI") cout << "Dữ liệu không hợp lệ. Thử lại.\n"; else cout << "Invalid input. Try again.\n";
                    continue;
                }
                if (!isfinite(val)) { if (acc.settings.language == "VI") cout << "Số không hợp lệ. Thử lại.\n"; else cout << "Invalid number. Try again.\n"; continue; }
                if (val < 0.0) { if (acc.settings.language == "VI") cout << "Không cho phép âm. Nhập 0-100.\n"; else cout << "Negative percentages are not allowed. Please enter a value between 0 and 100.\n"; continue; }
                if (val > 100.0) { if (acc.settings.language == "VI") cout << "Phần trăm không quá 100. Nhập 0-100.\n"; else cout << "Percent cannot exceed 100. Please enter a value between 0 and 100.\n"; continue; }
                attempted[k] = val;
                anyChange = true;
                break;
            }
        }

        // Validate totals
        double total = 0.0;
        for (auto &k : keys) total += attempted[k];
        if (total < -1e-9 || total > 100.0 + 1e-9) {
            if (acc.settings.language == "VI") cout << "Tổng phân bổ không hợp lệ: " << total << "%. Phải là 0-100. Nhập lại.\n";
            else cout << "Invalid allocation: categories sum to " << total << "%. Must be between 0 and 100. Please re-enter.\n";
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
        if (acc.settings.language == "VI") cout << "Đã cập nhật phân bổ. 'Other' là " << fixed << setprecision(2) << acc.allocationPct[nkOther] << "%\n";
        else cout << "Allocations updated. 'Other' set to remaining " << fixed << setprecision(2) << acc.allocationPct[nkOther] << "%\n";
        break;
    }
}

// Helper: interactive category creation for initial setup
void interactiveCategorySetup(Account &acc) {
    if (acc.settings.language == "VI") {
        cout << "\n--- Thiết lập loại ---\n";
        cout << "Nhập tên loại, mỗi dòng một loại. Nhấn Enter trên dòng trống để kết thúc.\n";
        cout << "Nếu không nhập, dùng mặc định.\n";
    } else {
        cout << "\n--- Category setup ---\n";
        cout << "Enter category display names, one per line. Press Enter on an empty line to finish.\n";
        cout << "If you enter no categories, defaults will be used.\n";
    }
    vector<string> newCats;
    while (true) {
        if (acc.settings.language == "VI") cout << "Tên loại (trống = hoàn tất): ";
        else cout << "Category name (empty = finish): ";
        string line;
        if (!getline(cin, line)) line.clear();
        trim_inplace(line);
        if (line.empty()) break;
        string sanitized = sanitizeDisplayName(line);
        newCats.push_back(sanitized);
    }
    if (newCats.empty()) {
        if (acc.settings.language == "VI") cout << "Không nhập loại. Dùng mặc định (Emergency, Entertainment, Saving, Other).\n";
        else cout << "No categories entered. Using defaults (Emergency, Entertainment, Saving, Other).\n";
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
    if (acc.settings.language == "VI") cout << "Đã tạo loại.\n";
    else cout << "Categories created.\n";
}

// Called from main when user chooses to set up new account
void runInitialSetup(Account &acc) {
    interactiveCategorySetup(acc);
    if (acc.settings.language == "VI") cout << "\nBạn có muốn thiết lập phần trăm phân bổ ngay không? (s = thiết lập, l = để sau) [s/l]: ";
    else cout << "\nWould you like to set allocation percentages now? (s = set now, l = leave and set later) [s/l]: ";
    string resp;
    if (!getline(cin, resp)) resp = "l";
    trim_inplace(resp);
    if (!resp.empty() && (resp[0]=='s' || resp[0]=='S')) {
        interactiveAllocSetup(acc, true);
    } else {
        if (acc.settings.language == "VI") cout << "Bạn có thể thiết lập phân bổ sau bằng mục 4.\n";
        else cout << "You can set allocations later from menu option 4.\n";
    }
    if (acc.settings.language == "VI") cout << "Hoàn tất thiết lập!\n";
    else cout << "You are all set!\n";
}

// Settings menu (updated to use numbers for toggles, single-enter return should work)
void settingsMenu(Account &acc) {
    // Ensure input buffer is clean at entry
    // (we use getline consistently; this reduces leftover newline problems)
    while (true) {
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
        if (acc.settings.language == "VI") {
            cout << "1. Tự động lưu: " << (acc.settings.autoSave ? "ON" : "OFF") << "\n";
            cout << "2. Tự động xử lý lịch & lãi khi khởi động: " << (acc.settings.autoProcessOnStartup ? "ON" : "OFF") << "\n";
            cout << "3. Ngôn ngữ: " << acc.settings.language << "\n";
            cout << "\tCác ngôn ngữ có sẵn:\n";
            cout << "\t- EN\n";
            cout << "\t- VI\n";
            cout << "(n) Nuke chương trình (đặt lại và xóa file lưu)\n";
            cout << "Nhấn Enter để quay lại menu chính.\n";
            cout << "Lựa chọn: ";
        } else {
            cout << "1. Auto-save: " << (acc.settings.autoSave ? "ON" : "OFF") << "\n";
            cout << "2. Auto process schedules & interest at startup: " << (acc.settings.autoProcessOnStartup ? "ON" : "OFF") << "\n";
            cout << "3. Language: " << acc.settings.language << "\n";
            cout << "\tAvailable languages:\n";
            cout << "\t- EN\n";
            cout << "\t- VI\n";
            cout << "(n) Nuke program (reset and delete save file)\n";
            cout << "Press Enter to go back to main menu.\n";
            cout << "Choice: ";
        }

        string ch;
        if (!getline(cin, ch)) ch.clear();
        trim_inplace(ch);
        if (ch.empty()) return; // single Enter returns immediately
        // Allow user to input number or letter
        if (ch == "1") {
            acc.settings.autoSave = !acc.settings.autoSave;
            if (acc.settings.language == "VI") cout << "Tự động lưu bây giờ " << (acc.settings.autoSave ? "ON" : "OFF") << ".\n";
            else cout << "Auto-save is now " << (acc.settings.autoSave ? "ON" : "OFF") << ".\n";
        } else if (ch == "2") {
            acc.settings.autoProcessOnStartup = !acc.settings.autoProcessOnStartup;
            if (acc.settings.language == "VI") cout << "Tự động xử lý khi khởi động bây giờ " << (acc.settings.autoProcessOnStartup ? "ON" : "OFF") << ".\n";
            else cout << "Auto process at startup is now " << (acc.settings.autoProcessOnStartup ? "ON" : "OFF") << ".\n";
        } else if (ch == "3") {
            // Toggle language between EN and VI
            if (acc.settings.language == "EN") acc.settings.language = "VI";
            else acc.settings.language = "EN";
            if (acc.settings.language == "VI") cout << "Ngôn ngữ đặt thành VI.\n";
            else cout << "Language set to EN.\n";
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
                        if (acc.settings.language == "VI") cout << "Đã xóa tệp lưu.\n";
                        else cout << "Save file removed.\n";
                    } else {
                        // reuse translation helper for message
                        cout << tr(acc.settings, "no_save_file");
                    }

                    // notify user and exit immediately
                    cout << tr(acc.settings, "nuke_done");
                    if (acc.settings.language == "VI") cout << "Thoát chương trình.\n";
                    else cout << "Exiting program.\n";

                    cout.flush(); // ensure messages are printed
                    exit(0);     // force immediate termination
                } else {
                    if (acc.settings.language == "VI") cout << tr(acc.settings, "nuke_cancel");
                    else cout << tr(acc.settings, "nuke_cancel");
                }
            } else {
                if (acc.settings.language == "VI") cout << "Lựa chọn không hợp lệ.\n";
                else cout << "Unknown option.\n";
            }
        }
    }
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
        if (acc.settings.language == "VI") cout << "Không thể mở tệp để tải. Không tìm thấy tệp1 lưu hiện có trong thư mục làm việc.\n";
        else cout << "Cannot open file to load. No existing save found in the working directory.\n";
        while (true) {
            if (acc.settings.language == "VI") cout << "Chọn: (s) thiết lập tài khoản mới, (r) thử tải lại tệp lưu sau khi đặt vào thư mục làm việc: ";
            else cout << "Choose: (s)et up new account, (r)etry loading save file after placing it in working directory: ";
            string resp;
            if (!getline(cin, resp)) { resp = "s"; }
            trim_inplace(resp);
            if (!resp.empty() && (resp[0]=='s' || resp[0]=='S')) {
                runInitialSetup(acc);
                break;
            } else if (!resp.empty() && (resp[0]=='r' || resp[0]=='R')) {
                if (acc.settings.language == "VI") cout << "Đang thử tải lại...\n";
                else cout << "Retrying load...\n";
                if (acc.loadFromFile()) {
                    break;
                } else {
                    if (acc.settings.language == "VI") cout << "Vẫn không tìm thấy tệp lưu. Bạn có thể đặt '" << SAVE_FILENAME << "' vào thư mục làm việc và chọn (r) lại, hoặc chọn (s) để thiết lập mới.\n";
                    else cout << "Still cannot find save file. You can place '" << SAVE_FILENAME << "' into the working directory and choose (r) again, or choose (s) to set up new.\n";
                    continue;
                }
            } else {
                if (acc.settings.language == "VI") cout << "Vui lòng trả lời 's' để thiết lập hoặc 'r' để thử lại.\n";
                else cout << "Please answer 's' to set up or 'r' to retry.\n";
            }
        }
    } else {
        // If loaded and auto-process setting is enabled, run it now
        if (acc.settings.autoProcessOnStartup) {
            if (acc.settings.language == "VI") cout << "Tự động xử lý lịch và lãi tại khởi động theo cài đặt...\n";
            else cout << "Auto-processing schedules and interest at startup as per settings...\n";
            acc.processSchedulesUpTo(today());
            acc.applyInterestUpTo(today());
            if (acc.settings.language == "VI") cout << "Hoàn tất tự động xử lý.\n";
            else cout << "Auto-processing complete.\n";
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
                if (acc.settings.language == "VI") cout << "Lựa chọn không hợp lệ.\n";
                else cout << "Invalid choice.\n";
                if (!askReturnToMenuOrSave(acc)) break;
                else continue;
            }

            if (choice == 1) {
                // --- Add manual transaction (improved category selection) ---
                if (acc.settings.language == "VI") cout << "Ngày (YYYY-MM-DD) [trống = hôm nay]: ";
                else cout << "Date (YYYY-MM-DD) [empty = today]: ";
                string dateStr;
                if (!getline(cin, dateStr)) dateStr.clear();
                trim_inplace(dateStr);
                chrono_tp d;
                if (dateStr.empty()) d = today();
                else { if (!tryParseDate(dateStr, d)) { if (acc.settings.language == "VI") cout << "Định dạng ngày không hợp lệ. Dùng YYYY-MM-DD.\n"; else cout << "Invalid date format. Use YYYY-MM-DD.\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; } }

                if (acc.settings.language == "VI") cout << "Số tiền (dương = thu nhập, âm = chi tiêu): ";
                else cout << "Amount (positive = income, negative = expense): ";
                double amt;
                string amtLine;
                if (!getline(cin, amtLine)) amtLine.clear();
                trim_inplace(amtLine);
                try {
                    amt = stod(amtLine);
                } catch (...) {
                    if (acc.settings.language == "VI") cout << "Số tiền không hợp lệ.\n";
                    else cout << "Invalid amount.\n";
                    if (!askReturnToMenuOrSave(acc)) break;
                    else continue;
                }

                vector<pair<string,string>> cats; // pair<display, normalized>
                for (auto &p : acc.displayNames) cats.emplace_back(p.second.empty() ? p.first : p.second, p.first);
                sort(cats.begin(), cats.end(), [](auto &a, auto &b){ return a.first < b.first; });

                if (acc.settings.language == "VI") cout << "Nhập loại hoặc số của nó, hoặc để trống để tự động phân bổ (cho thu nhập).\n";
                else cout << "Enter category or its number, or leave blank for Auto-allocate (for incomes).\n";
                if (acc.settings.language == "VI") cout << "Các loại hiện có:\n"; else cout << "Existing categories:\n";
                for (size_t i = 0; i < cats.size(); ++i) {
                    cout << "  " << (i+1) << ". " << cats[i].first << "\n";
                }
                if (acc.settings.language == "VI") cout << "Loại (tên hoặc số) [trống = Auto-allocate]: ";
                else cout << "Category (name or number) [blank = Auto-allocate]: ";
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
                            } else { if (acc.settings.language == "VI") cout << "Số ngoài phạm vi.\n"; else cout << "Number out of range.\n"; }
                        } catch (...) {}
                    }
                    if (!handled) {
                        string sanitized = sanitizeDisplayName(catInput);
                        string nk = normalizeKey(sanitized);
                        if (acc.displayNames.find(nk) != acc.displayNames.end()) {
                            chosenDisplayCat = acc.displayNames[nk];
                        } else {
                            while (true) {
                                if (acc.settings.language == "VI") cout << "Loại không tồn tại. Tạo \"" << sanitized << "\" hay nhập lại? (c/r): ";
                                else cout << "Category does not exist. Do you want to create \"" << sanitized << "\" or retype? (c/r): ";
                                string resp; if (!getline(cin, resp)) resp = "r";
                                trim_inplace(resp);
                                if (!resp.empty() && (resp[0]=='c' || resp[0]=='C')) {
                                    acc.displayNames[nk] = sanitized;
                                    if (acc.categoryBalances.find(nk) == acc.categoryBalances.end()) acc.categoryBalances[nk] = 0.0;
                                    if (acc.allocationPct.find(nk) == acc.allocationPct.end()) acc.allocationPct[nk] = 0.0;
                                    chosenDisplayCat = sanitized;
                                    break;
                                } else if (!resp.empty() && (resp[0]=='r' || resp[0]=='R')) {
                                    if (acc.settings.language == "VI") cout << "Nhập lại loại (tên hoặc số) [trống = Auto-allocate]: ";
                                    else cout << "Retype category (name or number) [blank = Auto-allocate]: ";
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
                                            } else if (acc.settings.language == "VI") cout << "Số ngoài phạm vi.\n"; else cout << "Number out of range.\n";
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
                                    if (acc.settings.language == "VI") cout << "Vui lòng trả lời 'c' để tạo hoặc 'r' để nhập lại.\n";
                                    else cout << "Please answer 'c' to create or 'r' to retype.\n";
                                }
                            }
                        }
                    }
                }

                if (acc.settings.language == "VI") cout << "Ghi chú: "; else cout << "Note: ";
                string note; if (!getline(cin, note)) note.clear();

                if (amt > 0.0 && willAutoAllocate) {
                    acc.allocateAmount(d, amt, note + " (manual income)");
                    if (acc.settings.language == "VI") cout << "Đã thêm và tự động phân bổ theo phần trăm.\n";
                    else cout << "Added and auto-allocated by percentages.\n";
                } else {
                    if (chosenDisplayCat.empty()) chosenDisplayCat = "Other";
                    string nkChosen = normalizeKey(sanitizeDisplayName(chosenDisplayCat));
                    if (acc.displayNames.find(nkChosen) == acc.displayNames.end()) acc.displayNames[nkChosen] = sanitizeDisplayName(chosenDisplayCat);
                    acc.addManualTransaction(d, amt, acc.displayNames[nkChosen], note);
                    if (acc.settings.language == "VI") cout << "Đã thêm.\n";
                    else cout << "Added.\n";
                }

            } else if (choice == 2) {
                if (acc.settings.language == "VI") cout << "Loại: 1) Mỗi X ngày  2) Hàng tháng vào ngày D\nLựa chọn: ";
                else cout << "Type: 1) Every X days  2) Monthly on day D\nChoice: ";
                string tline;
                if (!getline(cin, tline)) tline.clear();
                trim_inplace(tline);
                int t = 0;
                try { t = stoi(tline); } catch (...) { t = 0; }
                if (t != 1 && t != 2) {
                    if (acc.settings.language == "VI") cout << "Loại không hợp lệ.\n";
                    else cout << "Invalid type.\n";
                    if (!askReturnToMenuOrSave(acc)) break;
                    else continue;
                }

                Schedule s;
                if (t == 1) {
                    s.type = ScheduleType::EveryXDays;
                    if (acc.settings.language == "VI") cout << "Nhập khoảng ngày: ";
                    else cout << "Enter days interval: ";
                    string p; if (!getline(cin, p)) p.clear();
                    trim_inplace(p);
                    try { s.param = stoi(p); } catch (...) { s.param = 0; }
                    if (s.param <= 0) { if (acc.settings.language == "VI") cout << "Khoảng phải > 0.\n"; else cout << "Interval must be > 0.\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; }
                } else {
                    s.type = ScheduleType::MonthlyDay;
                    if (acc.settings.language == "VI") cout << "Nhập ngày trong tháng (1-31): ";
                    else cout << "Enter day of month (1-31): ";
                    string p; if (!getline(cin, p)) p.clear();
                    trim_inplace(p);
                    try { s.param = stoi(p); } catch (...) { s.param = 0; }
                    if (s.param < 1 || s.param > 31) { if (acc.settings.language == "VI") cout << "Ngày phải 1-31.\n"; else cout << "Day must be between 1 and 31.\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; }
                }

                if (acc.settings.language == "VI") cout << "Số tiền (dương = thu nhập định kỳ, âm = chi tiêu định kỳ): ";
                else cout << "Amount (positive for scheduled income, negative for scheduled expense): ";
                string amtLine;
                if (!getline(cin, amtLine)) amtLine.clear();
                trim_inplace(amtLine);
                try { s.amount = stod(amtLine); } catch (...) { if (acc.settings.language == "VI") cout << "Số tiền không hợp lệ.\n"; else cout << "Invalid amount.\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; }

                if (acc.settings.language == "VI") cout << "Ghi chú: "; else cout << "Note: ";
                if (!getline(cin, s.note)) s.note.clear();
                trim_inplace(s.note);

                if (acc.settings.language == "VI") cout << "Loại giao dịch hoặc để trống để dùng 'Other' (hoặc auto-alloc nếu phù hợp): ";
                else cout << "Category or blank for Other (or auto-alloc if appropriate): ";
                string catIn;
                if (!getline(cin, catIn)) catIn.clear();
                trim_inplace(catIn);
                if (!catIn.empty()) {
                    string sanitized = sanitizeDisplayName(catIn);
                    string nk = normalizeKey(sanitized);
                    if (acc.displayNames.find(nk) == acc.displayNames.end()) {
                        // ask to create
                        if (acc.settings.language == "VI") cout << "Loại không tồn tại. Tạo? (y/N): ";
                        else cout << "Category not present. Create? (y/N): ";
                        string resp; if (!getline(cin, resp)) resp.clear(); trim_inplace(resp);
                        if (!resp.empty() && (resp[0]=='y' || resp[0]=='Y')) {
                            acc.displayNames[nk] = sanitized;
                            if (acc.categoryBalances.find(nk) == acc.categoryBalances.end()) acc.categoryBalances[nk] = 0.0;
                        }
                    }
                    s.category = sanitized;
                }

                if (s.autoAllocate && s.amount < 0.0) {
                    if (acc.settings.language == "VI") cout << "Tự động phân bổ chỉ áp dụng cho số dương. Lịch này là số âm; sẽ ghi vào loại.\n";
                    else cout << "Auto-allocation only applies to positive amounts. This schedule is negative; it will be recorded to a category.\n";
                    if (s.category.empty()) s.category = "Other";
                    s.autoAllocate = false;
                }

                if (acc.settings.language == "VI") cout << "Ngày bắt đầu (YYYY-MM-DD) [hôm nay]: ";
                else cout << "Start date (YYYY-MM-DD) [today]: ";
                string start; if (!getline(cin, start)) start.clear();
                trim_inplace(start);
                if (start.empty()) s.nextDate = today();
                else { if (!tryParseDate(start, s.nextDate)) { if (acc.settings.language == "VI") cout << "Ngày bắt đầu không hợp lệ.\n"; else cout << "Invalid start date.\n"; if (!askReturnToMenuOrSave(acc)) break; else continue; } }

                acc.addSchedule(s);
                if (acc.settings.language == "VI") cout << "Đã thêm lịch.\n"; else cout << "Scheduled transaction added.\n";

            } else if (choice == 3) {
                acc.printSummary();

            } else if (choice == 4) {
                // New allocation setup: operate only on existing categories (Other is remainder)
                interactiveAllocSetup(acc, false);

            } else if (choice == 5) {
                acc.processSchedulesUpTo(today());
                if (acc.settings.language == "VI") cout << "Đã xử lý lịch.\n"; else cout << "Processed schedules.\n";

            } else if (choice == 6) {
                // New flow: let user choose to (A)dd/Update interest entry, (R)emove, or (P)lay now apply interest up to today
                if (acc.settings.language == "VI") cout << "Menu lãi: (a) thêm/cập nhật lãi, (r) xóa lãi, (p) xử lý/áp dụng lãi ngay: ";
                else cout << "Interest menu: (a)dd/update rate, (r)emove rate, (p)rocess/apply interest now: ";
                string sub;
                if (!getline(cin, sub)) sub.clear();
                trim_inplace(sub);
                if (!sub.empty() && (sub[0]=='a' || sub[0]=='A')) {
                    if (acc.settings.language == "VI") cout << "Nhập tên loại (hiển thị): ";
                    else cout << "Enter category display name: ";
                    string display;
                    if (!getline(cin, display)) display.clear();
                    trim_inplace(display);
                    if (display.empty()) { if (acc.settings.language == "VI") cout << "Tên không được để trống.\n"; else cout << "Name cannot be empty.\n"; continue; }
                    string nk = normalizeKey(display);
                    if (acc.displayNames.find(nk) == acc.displayNames.end()) acc.displayNames[nk] = sanitizeDisplayName(display);

                    if (acc.settings.language == "VI") cout << "Nhập lãi (vd: 0.5% hoặc 0.5): ";
                    else cout << "Enter rate (e.g. 0.5% or 0.5): ";
                    string rateLine;
                    if (!getline(cin, rateLine)) rateLine.clear();
                    trim_inplace(rateLine);
                    double pct;
                    if (!tryParseRate(rateLine, pct)) { if (acc.settings.language == "VI") cout << "Lãi không hợp lệ.\n"; else cout << "Invalid rate.\n"; continue; }

                    if (acc.settings.language == "VI") cout << "Loại tỉ lệ: (m) hàng tháng, (a) hàng năm: ";
                    else cout << "Rate type: (m) monthly, (a) annual: ";
                    string typ;
                    if (!getline(cin, typ)) typ.clear();
                    trim_inplace(typ);
                    bool isMonthly = (!typ.empty() && (typ[0]=='m' || typ[0]=='M'));

                    if (acc.settings.language == "VI") cout << "Ngày bắt đầu áp dụng (YYYY-MM-DD) [hôm nay]: ";
                    else cout << "Start date (YYYY-MM-DD) [today]: ";
                    string sdate; if (!getline(cin, sdate)) sdate.clear();
                    trim_inplace(sdate);
                    chrono_tp sdt = today();
                    if (!sdate.empty()) { if (!tryParseDate(sdate, sdt)) { if (acc.settings.language == "VI") cout << "Ngày không hợp lệ, dùng hôm nay.\n"; else cout << "Invalid date, using today.\n"; sdt = today(); } }

                    // set nextApplyDate to startDate by default
                    InterestEntry ie;
                    ie.categoryNormalized = nk;
                    ie.ratePct = pct;
                    ie.monthly = isMonthly;
                    ie.startDate = sdt;
                    ie.nextApplyDate = sdt;
                    acc.interestMap[nk] = ie;
                    if (acc.settings.language == "VI") cout << "Đã thêm/cập nhật lãi.\n"; else cout << "Added/updated interest entry.\n";
                    logChange("Interest add/update: " + display + " rate=" + to_string(pct) + (isMonthly? " monthly":" annual") + " start=" + toDateString(sdt));
                } else if (!sub.empty() && (sub[0]=='r' || sub[0]=='R')) {
                    if (acc.settings.language == "VI") cout << "Nhập tên loại để xóa lãi: ";
                    else cout << "Enter category name to remove interest: ";
                    string name; if (!getline(cin, name)) name.clear();
                    trim_inplace(name);
                    string nk = normalizeKey(name);
                    if (acc.interestMap.find(nk) != acc.interestMap.end()) {
                        acc.interestMap.erase(nk);
                        if (acc.settings.language == "VI") cout << "Đã xóa.\n"; else cout << "Removed.\n";
                        logChange("Interest removed for " + name);
                    } else {
                        if (acc.settings.language == "VI") cout << "Không tìm thấy.\n"; else cout << "Not found.\n";
                    }
                } else if (!sub.empty() && (sub[0]=='p' || sub[0]=='P')) {
                    acc.applyInterestUpTo(today());
                    if (acc.settings.language == "VI") cout << "Đã áp dụng lãi đến hôm nay.\n"; else cout << "Applied interest up to today.\n";
                } else {
                    if (acc.settings.language == "VI") cout << "Lựa chọn không hợp lệ.\n"; else cout << "Invalid choice.\n";
                }

            } else if (choice == 7) {
                acc.saveToFile();
            } else if (choice == 8) {
                if (!acc.loadFromFile()) {
                    if (acc.settings.language == "VI") cout << "Không thể tải tệp.\n";
                    else cout << "Failed to load save file.\n";
                }
            } else if (choice == 9) {
                if (acc.settings.language == "VI") cout << "Tạm biệt!\n"; else cout << "Goodbye!\n";
                break;
            } else if (choice == 10) {
                settingsMenu(acc);
            } else {
                if (acc.settings.language == "VI") cout << "Lựa chọn không hợp lệ.\n"; else cout << "Invalid choice.\n";
            }

            if (!askReturnToMenuOrSave(acc)) break;
        }
    }

    return 0;
}
