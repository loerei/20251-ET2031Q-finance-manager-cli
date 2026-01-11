// finance.cpp
// Personal Finance Manager CLI
// Features:
// - Manual transactions with notes and categories
// - Scheduled incomes (repeat every X days or monthly on specific day)
// - Auto allocation by percentage across categories
// - Basic saving interest calculation
// - Save/Load data from text file
//
// Build: g++ -std=c++17 finance.cpp -o finance

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

inline chrono_tp parseDate(const string &s) {
    // expects YYYY-MM-DD
    tm t = {};
    istringstream ss(s);
    ss >> get_time(&t, "%Y-%m-%d");
    return chrono::system_clock::from_time_t(mktime(&t));
}

inline string toDateString(const chrono_tp &tp) {
    time_t tt = chrono::system_clock::to_time_t(tp);
    tm *t = localtime(&tt);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d", t);
    return string(buf);
}

chrono_tp today() {
    auto now = chrono::system_clock::now();
    time_t tt = chrono::system_clock::to_time_t(now);
    tm *t = localtime(&tt);
    t->tm_hour = 0; t->tm_min = 0; t->tm_sec = 0;
    return chrono::system_clock::from_time_t(mktime(t));
}

int dayOfMonth(const chrono_tp &tp) {
    time_t tt = chrono::system_clock::to_time_t(tp);
    tm *t = localtime(&tt);
    return t->tm_mday;
}

chrono_tp addDays(const chrono_tp &tp, int days) {
    return tp + chrono::hours(24LL * days);
}

chrono_tp nextMonthlyOn(const chrono_tp &from, int day) {
    time_t tt = chrono::system_clock::to_time_t(from);
    tm t = *localtime(&tt);
    int curDay = t.tm_mday;
    if (curDay < day) t.tm_mday = day;
    else {
        t.tm_mday = day;
        t.tm_mon += 1;
    }
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
    return chrono::system_clock::from_time_t(mktime(&t));
}

struct Account {
    double balance = 0.0;
    vector<Transaction> txs;
    vector<Schedule> schedules;
    map<string, double> allocationPct; // category -> percent
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
            // Prevent runaway infinite loops: simple guard
            int guard = 0;
            while (s.nextDate <= upTo && guard < 10000) {
                if (s.autoAllocate) {
                    allocateAmount(s.amount, "Scheduled income: " + s.note);
                } else {
                    addManualTransaction(s.nextDate, s.amount, "Other", "Scheduled income: " + s.note);
                }
                if (s.type == ScheduleType::EveryXDays)
                    s.nextDate = addDays(s.nextDate, s.param);
                else
                    s.nextDate = nextMonthlyOn(s.nextDate, s.param);
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
        double savingBal = categoryBalances["Saving"];
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
        for (auto &p : allocationPct) ofs << p.first << "|" << p.second << "\n";
        ofs << "CATEGORIES\n";
        for (auto &p : categoryBalances) ofs << p.first << "|" << p.second << "\n";
        ofs << "SCHEDULES\n";
        for (auto &s : schedules) {
            ofs << (s.type==ScheduleType::EveryXDays? "E":"M") << "|"
                << s.param << "|" << s.amount << "|"
                << (s.autoAllocate ? "1" : "0") << "|" << toDateString(s.nextDate) << "|" << s.note << "\n";
        }
        ofs << "TXS\n";
        for (auto &t : txs) {
            ofs << toDateString(t.date) << "|" << t.amount << "|" << t.category << "|" << t.note << "\n";
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
        while (getline(ifs, line)) {
            if (line == "ALLOCATIONS") { sec = Alloc; continue; }
            if (line == "CATEGORIES") { sec = Cats; continue; }
            if (line == "SCHEDULES") { sec = Scheds; continue; }
            if (line == "TXS") { sec = Txs; continue; }
            if (line.rfind("BALANCE ", 0) == 0)
                balance = stod(line.substr(8));
            else if (line.rfind("SAVING_INTEREST ", 0) == 0)
                savingAnnualInterestPct = stod(line.substr(15));
            else {
                if (sec == Alloc) {
                    auto pos = line.find('|');
                    if (pos==string::npos) continue;
                    string k = line.substr(0,pos);
                    double v = stod(line.substr(pos+1));
                    allocationPct[k] = v;
                } else if (sec == Cats) {
                    auto pos = line.find('|');
                    if (pos==string::npos) continue;
                    string k = line.substr(0,pos);
                    double v = stod(line.substr(pos+1));
                    categoryBalances[k] = v;
                } else if (sec == Scheds) {
                    vector<string> parts;
                    string tmp; istringstream ss(line);
                    while (getline(ss, tmp, '|')) parts.push_back(tmp);
                    if (parts.size() >= 6) {
                        Schedule s;
                        s.type = (parts[0]=="E")? ScheduleType::EveryXDays : ScheduleType::MonthlyDay;
                        s.param = stoi(parts[1]);
                        s.amount = stod(parts[2]);
                        s.autoAllocate = (parts[3] == "1" || parts[3] == "true");
                        s.nextDate = parseDate(parts[4]);
                        s.note = parts[5];
                        schedules.push_back(s);
                    }
                } else if (sec == Txs) {
                    vector<string> parts;
                    string tmp; istringstream ss(line);
                    while (getline(ss, tmp, '|')) parts.push_back(tmp);
                    if (parts.size() >= 4) {
                        Transaction t;
                        t.date = parseDate(parts[0]);
                        t.amount = stod(parts[1]);
                        t.category = parts[2];
                        t.note = parts[3];
                        txs.push_back(t);
                    }
                }
            }
        }
        ifs.close();
        cout << "Loaded from " << filename << "\n";
    }
};

void printStartingGuide() {
    cout << "\n=== Starting Guide ===\n";
    cout << "This is a simple CLI personal finance manager.\n\n";
    cout << "H) Starting Guide - show this help message.\n";
    cout << "1) Add manual transaction - specify date, amount (+ income / - expense), category, note.\n";
    cout << "   If income, you can auto-allocate by your defined percentages.\n";
    cout << "2) Add scheduled income - recurring income either every X days or monthly on day D.\n";
    cout << "3) Show summary - shows total balance, category balances, allocations and recent txs.\n";
    cout << "4) Set allocation percentages - define how income is split across categories.\n";
    cout << "5) Process schedules up to today - apply scheduled incomes that are due.\n";
    cout << "6) Apply monthly saving interest - set rate (annual %) and apply monthly interest.\n";
    cout << "7) Save - write current data to " << SAVE_FILENAME << ".\n";
    cout << "8) Load - load data from " << SAVE_FILENAME << ".\n";
    cout << "9) Exit - quit program.\n";
    cout << "10) Drop a nuke - resets the program to initial state and deletes the save file (confirmation required).\n";
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

int main() {
    ios::sync_with_stdio(false);
    // Keep cin tied to cout so prompts are flushed before waiting for input.
    cin.tie(&cout);
    Account acc;
    acc.loadFromFile();

    while (true) {
        printMenu();
        string choiceStr;
        if (!getline(cin, choiceStr)) break;
        trim_inplace(choiceStr);
        if (choiceStr.empty()) continue;

        // Handle H/h for starting guide
        if (choiceStr == "H" || choiceStr == "h") {
            printStartingGuide();
            continue;
        }

        // Try parse integer option
        int choice = -1;
        try {
            choice = stoi(choiceStr);
        } catch (...) {
            cout << "Invalid choice.\n";
            continue;
        }

        if (choice == 1) {
            string dateStr;
            cout << "Date (YYYY-MM-DD) [empty = today]: ";
            getline(cin, dateStr);
            chrono_tp d = dateStr.empty() ? today() : parseDate(dateStr);
            cout << "Amount (positive = income, negative = expense): ";
            double amt; if (!(cin >> amt)) { cout << "Invalid amount.\n"; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); continue; }
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
            int t; if (!(cin >> t)) { cout << "Invalid type.\n"; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); continue; }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            Schedule s;
            if (t == 1) {
                s.type = ScheduleType::EveryXDays;
                cout << "Enter days interval: "; if (!(cin >> s.param)) { cout << "Invalid interval.\n"; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); continue; }
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            } else {
                s.type = ScheduleType::MonthlyDay;
                cout << "Enter day of month (1-31): "; if (!(cin >> s.param)) { cout << "Invalid day.\n"; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); continue; }
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            }
            cout << "Amount (positive): "; if (!(cin >> s.amount)) { cout << "Invalid amount.\n"; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); continue; }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Note: "; getline(cin, s.note);
            cout << "Auto-allocate? (y/n) [y]: ";
            string tmp; getline(cin, tmp);
            s.autoAllocate = (tmp.empty() || tmp[0]=='y' || tmp[0]=='Y');
            cout << "Start date (YYYY-MM-DD) [today]: ";
            string start; getline(cin, start);
            s.nextDate = start.empty() ? today() : parseDate(start);
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
            if (!sline.empty()) acc.savingAnnualInterestPct = stod(sline);
            acc.applyMonthlySavingInterest();
            cout << "Interest applied.\n";
        } else if (choice == 7) {
            acc.saveToFile();
        } else if (choice == 8) {
            acc.loadFromFile();
        } else if (choice == 9) {
            cout << "Goodbye.\n";
            break;
        } else if (choice == 10) {
            cout << "Do you really want to nuke your program? (Everything will returns to its initial state) [y/N]: ";
            string resp;
            getline(cin, resp);
            if (!resp.empty() && (resp[0]=='y' || resp[0]=='Y')) {
                // Reset account to initial state
                acc = Account();
                // Remove save file if exists
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
    }

    return 0;
}
