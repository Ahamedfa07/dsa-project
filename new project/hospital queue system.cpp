// ============================================================
//  Hospital Emergency Queue System  (DSA Project, C++17)
//  Data structures: Priority Queue (Heap), Hash Map, Queue, Vector
//  Compile:  g++ -std=c++17 main.cpp -o hospital
// ============================================================
#include <iostream>
#include <queue>
#include <vector>
#include <unordered_map>
#include <string>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
using namespace std;

// ---------- ANSI colours ----------
const string RESET = "\033[0m";
string color(int severity) {
    switch (severity) {
        case 5:  return "\033[1;31m";   // bold red
        case 4:  return "\033[0;91m";   // light red
        case 3:  return "\033[0;33m";   // yellow
        case 2:  return "\033[0;32m";   // green
        default: return "\033[0;92m";   // light green
    }
}

// ---------- Patient ----------
struct Patient {
    int id;
    string name;
    int age;
    int severity;          // 1 (minor) to 5 (critical)
    time_t arrivalTime;
    string condition;
};

// ---------- Aging settings ----------
const double AGING_SECONDS = 600.0;   // 10 நிமிஷம் காத்திருந்தா priority +1
bool agingEnabled = true;

double priorityScore(const Patient& p, time_t now) {
    double score = p.severity;
    if (agingEnabled) score += difftime(now, p.arrivalTime) / AGING_SECONDS;
    return score;
}

// ---------- Comparator for the heap ----------
// true return பண்ணா => a-க்கு b-ஐ விட குறைந்த priority
struct Compare {
    time_t now;
    Compare(time_t n = time(nullptr)) : now(n) {}
    bool operator()(const Patient& a, const Patient& b) const {
        double sa = priorityScore(a, now);
        double sb = priorityScore(b, now);
        if (sa == sb) {
            if (a.arrivalTime == b.arrivalTime) return a.id > b.id;  // ஒரே second-ல வந்தா சிறிய ID முதல்
            return a.arrivalTime > b.arrivalTime;                    // முதல்ல வந்தவர் முதல்
        }
        return sa < sb;                                              // அதிக score முதல்
    }
};

// ---------- Helper functions ----------
string fmtTime(double seconds) {
    long s = (long)seconds;
    ostringstream os;
    os << s / 60 << "m " << setw(2) << setfill('0') << s % 60 << "s";
    return os.str();
}

string clean(string s) {                 // file separator '|' வராம பாதுகாக்க
    replace(s.begin(), s.end(), '|', '/');
    return s;
}

vector<string> split(const string& s, char d) {
    vector<string> out;
    string tok;
    stringstream ss(s);
    while (getline(ss, tok, d)) out.push_back(tok);
    return out;
}

int readInt(const string& prompt, int lo, int hi) {
    while (true) {
        cout << prompt;
        string line;
        if (!getline(cin, line)) exit(0);
        try {
            size_t idx;
            int v = stoi(line, &idx);
            if (idx == line.size() && v >= lo && v <= hi) return v;
        } catch (...) {}
        cout << "Invalid input. Enter a number between " << lo << " and " << hi << ".\n";
    }
}

string readLine(const string& prompt) {
    while (true) {
        cout << prompt;
        string line;
        if (!getline(cin, line)) exit(0);
        if (!line.empty()) return clean(line);
        cout << "Cannot be empty.\n";
    }
}

// ---------- Hospital class ----------
class Hospital {
private:
    typedef priority_queue<Patient, vector<Patient>, Compare> PQ;

    PQ pq{Compare(time(nullptr))};            // waiting patients (heap)
    unordered_map<int, Patient> patientMap;   // ID -> waiting patient
    queue<Patient> dischargeLog;              // treat ஆனவங்க (இந்த session)
    vector<double> waitTimes;                 // report-க்கு
    vector<int> treatedSeverity;              // report-க்கு
    int nextId = 1001;

    // Time மாறினா scores மாறும், அதனால treat/view முன்னாடி heap-ஐ rebuild பண்ணுவோம்
    void rebuildHeap() {
        pq = PQ(Compare(time(nullptr)));
        for (auto& kv : patientMap) pq.push(kv.second);
    }

    void printHeader() {
        cout << left << setw(7) << "ID" << setw(16) << "Name" << setw(6) << "Age"
             << setw(10) << "Severity" << setw(12) << "Waiting" << "Condition\n";
        cout << string(70, '-') << "\n";
    }

    void printRow(const Patient& p, time_t now) {
        cout << left << setw(7) << p.id << setw(16) << p.name << setw(6) << p.age
             << color(p.severity) << setw(10) << p.severity << RESET
             << setw(12) << fmtTime(difftime(now, p.arrivalTime))
             << p.condition << "\n";
    }

public:
    // ---- 1. Add ----
    void addPatient() {
        Patient p;
        p.id = nextId++;
        p.name = readLine("Name: ");
        p.age = readInt("Age: ", 0, 120);
        p.condition = readLine("Condition: ");
        p.severity = readInt("Severity (1-minor ... 5-critical): ", 1, 5);
        p.arrivalTime = time(nullptr);

        pq.push(p);
        patientMap[p.id] = p;
        cout << "Patient added. ID = " << p.id << "\n";
    }

    // ---- 2. Treat next ----
    void treatNext() {
        if (patientMap.empty()) { cout << "No patients waiting.\n"; return; }
        rebuildHeap();
        Patient p = pq.top();
        pq.pop();
        patientMap.erase(p.id);

        double wait = difftime(time(nullptr), p.arrivalTime);
        waitTimes.push_back(wait);
        treatedSeverity.push_back(p.severity);
        dischargeLog.push(p);

        cout << "Treating: " << p.name << " (ID " << p.id << ", "
             << color(p.severity) << "Severity " << p.severity << RESET << ")\n";
        cout << "Condition: " << p.condition << "\n";
        cout << "Waited: " << fmtTime(wait) << "\n";
    }

    // ---- 3. View waiting list ----
    void viewWaiting() {
        if (patientMap.empty()) { cout << "Queue is empty.\n"; return; }
        rebuildHeap();
        PQ temp = pq;                         // copy; original பாதிக்காது
        time_t now = time(nullptr);
        printHeader();
        while (!temp.empty()) {
            printRow(temp.top(), now);
            temp.pop();
        }
        cout << "Total waiting: " << patientMap.size() << "\n";
    }

    // ---- 4. Search by ID ----
    void searchPatient() {
        int id = readInt("Enter patient ID: ", 0, 2000000000);
        auto it = patientMap.find(id);
        if (it == patientMap.end()) {
            cout << "Not found (or already treated).\n";
            return;
        }
        printHeader();
        printRow(it->second, time(nullptr));
    }

    // ---- 5. Update severity ----
    void updateSeverity() {
        int id = readInt("Patient ID: ", 0, 2000000000);
        auto it = patientMap.find(id);
        if (it == patientMap.end()) { cout << "Not found.\n"; return; }
        cout << "Current severity: " << it->second.severity << "\n";
        it->second.severity = readInt("New severity (1-5): ", 1, 5);
        rebuildHeap();
        cout << "Severity updated.\n";
    }

    // ---- 6. Daily report ----
    void dailyReport() {
        cout << "\n========== DAILY REPORT ==========\n";
        cout << "Still waiting   : " << patientMap.size() << "\n";
        cout << "Total treated   : " << waitTimes.size() << "\n";
        if (waitTimes.empty()) return;

        double sum = 0, longest = 0;
        for (double w : waitTimes) { sum += w; longest = max(longest, w); }
        int cnt[6] = {0};
        for (int s : treatedSeverity) cnt[s]++;

        cout << "Average wait    : " << fmtTime(sum / waitTimes.size()) << "\n";
        cout << "Longest wait    : " << fmtTime(longest) << "\n";
        cout << "Treated by severity:\n";
        for (int s = 5; s >= 1; s--)
            cout << "  " << color(s) << "Severity " << s << RESET << " : " << cnt[s] << "\n";
    }

    // ---- 7. Discharge log ----
    void viewDischarged() {
        if (dischargeLog.empty()) { cout << "No one treated in this session.\n"; return; }
        queue<Patient> temp = dischargeLog;
        cout << left << setw(7) << "ID" << setw(16) << "Name" << setw(10) << "Severity"
             << "Condition\n" << string(55, '-') << "\n";
        while (!temp.empty()) {
            Patient p = temp.front(); temp.pop();
            cout << left << setw(7) << p.id << setw(16) << p.name
                 << setw(10) << p.severity << p.condition << "\n";
        }
    }

    // ---- 8. Toggle aging ----
    void toggleAging() {
        agingEnabled = !agingEnabled;
        cout << "Aging is now " << (agingEnabled ? "ON" : "OFF") << "\n";
        rebuildHeap();
    }

    // ---- File save ----
    void saveToFile(const string& filename = "patients.txt") {
        ofstream f(filename);
        if (!f) { cout << "Warning: could not save data.\n"; return; }
        f << "N|" << nextId << "\n";
        for (auto& kv : patientMap) {
            const Patient& p = kv.second;
            f << "P|" << p.id << "|" << p.name << "|" << p.age << "|"
              << p.severity << "|" << p.arrivalTime << "|" << p.condition << "\n";
        }
        for (size_t i = 0; i < waitTimes.size(); i++)
            f << "T|" << waitTimes[i] << "|" << treatedSeverity[i] << "\n";
    }

    // ---- File load ----
    void loadFromFile(const string& filename = "patients.txt") {
        ifstream f(filename);
        if (!f) return;                       // முதல் தடவை: file இல்லை
        string line;
        while (getline(f, line)) {
            vector<string> t = split(line, '|');
            try {
                if (t[0] == "N" && t.size() >= 2) {
                    nextId = stoi(t[1]);
                } else if (t[0] == "P" && t.size() >= 7) {
                    Patient p;
                    p.id = stoi(t[1]);
                    p.name = t[2];
                    p.age = stoi(t[3]);
                    p.severity = stoi(t[4]);
                    p.arrivalTime = (time_t)stoll(t[5]);
                    p.condition = t[6];
                    patientMap[p.id] = p;
                } else if (t[0] == "T" && t.size() >= 3) {
                    waitTimes.push_back(stod(t[1]));
                    treatedSeverity.push_back(stoi(t[2]));
                }
            } catch (...) {
                // கெட்டுப்போன line-ஐ தவிர்த்துவிடு
            }
        }
        rebuildHeap();
        cout << "Loaded " << patientMap.size() << " waiting patient(s) from file.\n";
    }
};

// ---------- main ----------
int main() {
    Hospital h;
    h.loadFromFile();

    int choice;
    do {
        cout << "\n========== HOSPITAL EMERGENCY QUEUE ==========\n"
             << "1. Add Patient\n"
             << "2. Treat Next Patient\n"
             << "3. View Waiting List\n"
             << "4. Search Patient by ID\n"
             << "5. Update Severity\n"
             << "6. Daily Report\n"
             << "7. View Discharged Patients\n"
             << "8. Toggle Aging (starvation prevention)\n"
             << "0. Save & Exit\n";
        choice = readInt("Choice: ", 0, 8);

        switch (choice) {
            case 1: h.addPatient();     break;
            case 2: h.treatNext();      break;
            case 3: h.viewWaiting();    break;
            case 4: h.searchPatient();  break;
            case 5: h.updateSeverity(); break;
            case 6: h.dailyReport();    break;
            case 7: h.viewDischarged(); break;
            case 8: h.toggleAging();    break;
            case 0: cout << "Data saved. Bye!\n"; break;
        }
        h.saveToFile();                       // ஒவ்வொரு செயலுக்கும் பிறகு save
    } while (choice != 0);

    return 0;
}