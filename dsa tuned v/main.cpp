// ============================================================
//  Hospital Emergency Triage System  (DSA Project, C++17)
//
//  Data structures used
//    - Max-Heap (custom, array based)  : waiting patients by priority
//    - Hash Map (unordered_map)        : patient lookup by ID, doctors by specialty
//    - Queue (FIFO)                    : discharge log, round-robin doctors
//    - Dynamic array (vector)          : statistics, sorting for display
//
//  Compile:  g++ -std=c++17 -O2 main.cpp -o hospital
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
#include <chrono>
#include <random>
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
    int id = 0;
    string name;
    int age = 0;
    int severity = 1;          // 1 (minor) .. 5 (critical)
    time_t arrivalTime = 0;
    string condition;
    string specialty = "General";
    int version = 0;           // bumped on every severity update (lazy deletion)
};

struct Doctor {
    int id;
    string name;
    string specialty;
};

// ---------- Priority settings ----------
const double AGING_SECONDS = 600.0;   // every 10 minutes of waiting adds +1 to the score
const double AGE_BOOST     = 0.5;     // small boost for children (<=5) and elderly (>=65)
bool agingEnabled = true;

double ageBoost(int age) { return (age <= 5 || age >= 65) ? AGE_BOOST : 0.0; }

// Score shown to the user (depends on the current time).
double currentScore(const Patient& p, time_t now) {
    double s = p.severity + ageBoost(p.age);
    if (agingEnabled) s += difftime(now, p.arrivalTime) / AGING_SECONDS;
    return s;
}

// Heap key (does NOT depend on the current time).
//   currentScore(p, now) = key(p) + now / AGING_SECONDS   (same constant for everyone)
// so comparing keys gives exactly the same order as comparing live scores,
// and the heap never has to be rebuilt as time passes.
double priorityKey(const Patient& p) {
    double k = p.severity + ageBoost(p.age);
    if (agingEnabled) k -= (double)p.arrivalTime / AGING_SECONDS;
    return k;
}

// true if a must be served before b
bool higher(const Patient& a, const Patient& b) {
    double ka = priorityKey(a), kb = priorityKey(b);
    if (ka != kb) return ka > kb;                       // higher priority first
    if (a.arrivalTime != b.arrivalTime)
        return a.arrivalTime < b.arrivalTime;           // earlier arrival first
    return a.id < b.id;                                 // smaller ID first
}

// ============================================================
//  Custom Max-Heap  (array representation)
//  parent(i) = (i-1)/2     left(i) = 2i+1     right(i) = 2i+2
// ============================================================
class PatientHeap {
private:
    vector<Patient> a;

    void siftUp(size_t i) {                             // O(log n)
        while (i > 0) {
            size_t p = (i - 1) / 2;
            if (!higher(a[i], a[p])) break;
            swap(a[i], a[p]);
            i = p;
        }
    }

    void siftDown(size_t i) {                           // O(log n)
        size_t n = a.size();
        while (true) {
            size_t l = 2 * i + 1, r = l + 1, best = i;
            if (l < n && higher(a[l], a[best])) best = l;
            if (r < n && higher(a[r], a[best])) best = r;
            if (best == i) break;
            swap(a[i], a[best]);
            i = best;
        }
    }

public:
    bool empty() const { return a.empty(); }
    size_t size() const { return a.size(); }
    void clear() { a.clear(); }

    void push(const Patient& p) {                       // O(log n)
        a.push_back(p);
        siftUp(a.size() - 1);
    }

    const Patient& top() const { return a[0]; }         // O(1)

    void pop() {                                        // O(log n)
        a[0] = a.back();
        a.pop_back();
        if (!a.empty()) siftDown(0);
    }

    // Bottom-up heap construction: O(n)
    void buildFrom(vector<Patient> v) {
        a = move(v);
        for (long i = (long)a.size() / 2 - 1; i >= 0; --i) siftDown((size_t)i);
    }
};

// ---------- Helper functions ----------
string fmtTime(double seconds) {
    long s = (long)seconds;
    ostringstream os;
    os << s / 60 << "m " << setw(2) << setfill('0') << s % 60 << "s";
    return os.str();
}

string clean(string s) {                 // keep the file separator '|' out of user text
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

// ============================================================
//  Hospital
// ============================================================
class Hospital {
private:
    PatientHeap heap;                                   // waiting patients (may hold stale copies)
    unordered_map<int, Patient> waiting;                // ID -> the ONE valid waiting record
    queue<Patient> dischargeLog;                        // treated this session (FIFO)
    vector<double> waitTimes;                           // for the report
    vector<int> treatedSeverity;                        // for the report
    unordered_map<string, queue<Doctor>> doctors;       // specialty -> doctors (round-robin)
    vector<string> specialties;                         // for the menu
    int nextId = 1001;
    bool dirty = false;

    // ---------- doctors ----------
    void addDoctor(const Doctor& d) {
        if (doctors.find(d.specialty) == doctors.end()) specialties.push_back(d.specialty);
        doctors[d.specialty].push(d);
    }

    void loadDoctors(const string& filename = "doctors.txt") {
        ifstream f(filename);
        if (!f) {                                       // first run: create a default file
            ofstream out(filename);
            out << "1|Perera|Cardiology\n2|Silva|Neurology\n3|Fernando|Orthopedics\n"
                << "4|Jayasinghe|General\n5|Bandara|General\n6|Wickrama|Pediatrics\n";
            out.close();
            f.open(filename);
        }
        string line;
        while (getline(f, line)) {
            vector<string> t = split(line, '|');
            if (t.size() < 3) continue;
            try { addDoctor({stoi(t[0]), t[1], t[2]}); } catch (...) {}
        }
        if (doctors.find("General") == doctors.end()) addDoctor({0, "Duty Doctor", "General"});
    }

    // Round-robin: take the front doctor, send him/her to the back of the queue. O(1)
    string assignDoctor(const string& spec) {
        auto it = doctors.find(spec);
        if (it == doctors.end() || it->second.empty()) it = doctors.find("General");
        if (it == doctors.end() || it->second.empty()) return "No doctor available";
        Doctor d = it->second.front();
        it->second.pop();
        it->second.push(d);
        return "Dr. " + d.name + " (" + d.specialty + ")";
    }

    // ---------- heap + hash map ----------
    void insertPatient(const Patient& p) {              // O(log n)
        waiting[p.id] = p;
        heap.push(p);
        dirty = true;
    }

    // Pop until a VALID (non-stale) patient is found. Each stale copy is removed once,
    // so the cost is O(log n) amortised.
    bool popValid(Patient& out) {
        while (!heap.empty()) {
            Patient p = heap.top();
            heap.pop();
            auto it = waiting.find(p.id);
            if (it != waiting.end() && it->second.version == p.version) {
                out = p;
                return true;
            }
        }
        return false;
    }

    void rebuildHeap() {                                // O(n) - only when aging is toggled
        vector<Patient> v;
        v.reserve(waiting.size());
        for (auto& kv : waiting) v.push_back(kv.second);
        heap.buildFrom(move(v));
    }

    // ---------- printing ----------
    void printHeader() {
        cout << left << setw(7) << "ID" << setw(16) << "Name" << setw(6) << "Age"
             << setw(10) << "Severity" << setw(8) << "Score" << setw(12) << "Waiting"
             << setw(13) << "Specialty" << "Condition\n";
        cout << string(86, '-') << "\n";
    }

    void printRow(const Patient& p, time_t now) {
        cout << left << setw(7) << p.id << setw(16) << p.name << setw(6) << p.age
             << color(p.severity) << setw(10) << p.severity << RESET
             << setw(8) << fixed << setprecision(2) << currentScore(p, now)
             << setw(12) << fmtTime(difftime(now, p.arrivalTime))
             << setw(13) << p.specialty << p.condition << "\n";
    }

    string chooseSpecialty() {
        cout << "Specialty needed:\n";
        for (size_t i = 0; i < specialties.size(); i++)
            cout << "  " << i + 1 << ". " << specialties[i] << "\n";
        int c = readInt("Choice: ", 1, (int)specialties.size());
        return specialties[(size_t)c - 1];
    }

public:
    Hospital() { loadDoctors(); }

    // ---- 1. Add ----
    void addPatient() {
        Patient p;
        p.id = nextId++;
        p.name = readLine("Name: ");
        p.age = readInt("Age: ", 0, 120);
        p.condition = readLine("Condition: ");
        p.severity = readInt("Severity (1-minor ... 5-critical): ", 1, 5);
        p.specialty = chooseSpecialty();
        p.arrivalTime = time(nullptr);
        insertPatient(p);
        cout << "Patient added. ID = " << p.id << "\n";
    }

    // ---- 2. Treat next ----
    void treatNext() {
        Patient p;
        if (!popValid(p)) { cout << "No patients waiting.\n"; return; }
        waiting.erase(p.id);
        dirty = true;

        double wait = difftime(time(nullptr), p.arrivalTime);
        waitTimes.push_back(wait);
        treatedSeverity.push_back(p.severity);
        dischargeLog.push(p);

        cout << "Treating: " << p.name << " (ID " << p.id << ", "
             << color(p.severity) << "Severity " << p.severity << RESET << ")\n";
        cout << "Condition: " << p.condition << "\n";
        cout << "Assigned to: " << assignDoctor(p.specialty) << "\n";
        cout << "Waited: " << fmtTime(wait) << "\n";
    }

    // ---- 3. View waiting list ----
    void viewWaiting() {
        if (waiting.empty()) { cout << "Queue is empty.\n"; return; }
        vector<Patient> v;
        v.reserve(waiting.size());
        for (auto& kv : waiting) v.push_back(kv.second);
        sort(v.begin(), v.end(), higher);               // O(n log n), heap is untouched
        time_t now = time(nullptr);
        printHeader();
        for (auto& p : v) printRow(p, now);
        cout << "Total waiting: " << waiting.size() << "\n";
    }

    // ---- 4. Search by ID ----
    void searchPatient() {
        int id = readInt("Enter patient ID: ", 0, 2000000000);
        auto it = waiting.find(id);                     // O(1) average
        if (it == waiting.end()) {
            cout << "Not found (or already treated).\n";
            return;
        }
        printHeader();
        printRow(it->second, time(nullptr));
    }

    // ---- 5. Update severity (lazy deletion, O(log n)) ----
    void updateSeverity() {
        int id = readInt("Patient ID: ", 0, 2000000000);
        auto it = waiting.find(id);
        if (it == waiting.end()) { cout << "Not found.\n"; return; }
        cout << "Current severity: " << it->second.severity << "\n";
        it->second.severity = readInt("New severity (1-5): ", 1, 5);
        it->second.version++;                           // old heap copy becomes stale
        heap.push(it->second);                          // push the fresh copy
        dirty = true;
        cout << "Severity updated.\n";
    }

    // ---- 6. Daily report ----
    void dailyReport() {
        cout << "\n========== DAILY REPORT ==========\n";
        cout << "Still waiting   : " << waiting.size() << "\n";
        cout << "Total treated   : " << waitTimes.size() << "\n";
        if (waitTimes.empty()) return;

        double sum = 0, longest = 0;
        for (double w : waitTimes) { sum += w; longest = max(longest, w); }
        int cnt[6] = {0};
        for (int s : treatedSeverity) cnt[s]++;

        cout << "Average wait    : " << fmtTime(sum / (double)waitTimes.size()) << "\n";
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
        rebuildHeap();                                  // keys changed, so rebuild once: O(n)
        dirty = true;
        cout << "Aging is now " << (agingEnabled ? "ON" : "OFF") << "\n";
    }

    // ---- 9. Bulk load: name|age|severity|condition|specialty ----
    void loadBulk(const string& filename = "sample_patients.txt") {
        ifstream f(filename);
        if (!f) { cout << "File '" << filename << "' not found. Use option 10 to generate one.\n"; return; }
        vector<Patient> batch;
        string line;
        while (getline(f, line)) {
            vector<string> t = split(line, '|');
            if (t.size() < 5) continue;
            try {
                Patient p;
                p.name = t[0];
                p.age = stoi(t[1]);
                p.severity = stoi(t[2]);
                p.condition = t[3];
                p.specialty = t[4];
                if (p.age < 0 || p.age > 120 || p.severity < 1 || p.severity > 5) continue;
                batch.push_back(p);
            } catch (...) {}
        }
        time_t now = time(nullptr);
        auto t0 = chrono::steady_clock::now();
        for (size_t i = 0; i < batch.size(); i++) {
            batch[i].id = nextId++;
            batch[i].arrivalTime = now - (time_t)(batch.size() - i);   // earlier line = earlier arrival
            insertPatient(batch[i]);
        }
        double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
        cout << "Loaded " << batch.size() << " patients from file in "
             << fixed << setprecision(2) << ms << " ms.\n";
    }

    // ---- 10. Generate a big sample file ----
    void generateSample(const string& filename = "sample_patients.txt") {
        int n = readInt("How many patients to generate (1-1000000): ", 1, 1000000);
        mt19937 rng(42);
        vector<string> conds = {"Chest pain", "Fracture", "Fever", "Head injury", "Cut wound",
                                "Breathing difficulty", "Stomach ache", "Burn", "Seizure", "Sprain"};
        ofstream out(filename);
        for (int i = 0; i < n; i++) {
            const string& sp = specialties[rng() % specialties.size()];
            out << "Patient" << i + 1 << "|" << (int)(rng() % 90 + 1) << "|"
                << (int)(rng() % 5 + 1) << "|" << conds[rng() % conds.size()] << "|" << sp << "\n";
        }
        cout << "Generated " << n << " patients in " << filename << "\n";
    }

    // ---- File save ----
    void saveIfDirty(const string& filename = "patients.txt") {
        if (!dirty) return;
        ofstream f(filename);
        if (!f) { cout << "Warning: could not save data.\n"; return; }
        f << "N|" << nextId << "|" << (agingEnabled ? 1 : 0) << "\n";
        for (auto& kv : waiting) {
            const Patient& p = kv.second;
            f << "P|" << p.id << "|" << p.name << "|" << p.age << "|"
              << p.severity << "|" << p.arrivalTime << "|" << p.specialty << "|"
              << p.condition << "\n";
        }
        for (size_t i = 0; i < waitTimes.size(); i++)
            f << "T|" << waitTimes[i] << "|" << treatedSeverity[i] << "\n";
        dirty = false;
    }

    // ---- File load ----
    void loadFromFile(const string& filename = "patients.txt") {
        ifstream f(filename);
        if (!f) return;                                 // first run: no file yet
        string line;
        while (getline(f, line)) {
            vector<string> t = split(line, '|');
            if (t.empty()) continue;
            try {
                if (t[0] == "N" && t.size() >= 2) {
                    nextId = stoi(t[1]);
                    if (t.size() >= 3) agingEnabled = (t[2] == "1");
                } else if (t[0] == "P" && t.size() >= 8) {
                    Patient p;
                    p.id = stoi(t[1]);
                    p.name = t[2];
                    p.age = stoi(t[3]);
                    p.severity = stoi(t[4]);
                    p.arrivalTime = (time_t)stoll(t[5]);
                    p.specialty = t[6];
                    p.condition = t[7];
                    if (p.severity < 1 || p.severity > 5 || p.age < 0 || p.age > 120) continue;
                    waiting[p.id] = p;
                    if (p.id >= nextId) nextId = p.id + 1;
                } else if (t[0] == "T" && t.size() >= 3) {
                    int sev = stoi(t[2]);
                    if (sev < 1 || sev > 5) continue;
                    waitTimes.push_back(stod(t[1]));
                    treatedSeverity.push_back(sev);
                }
            } catch (...) {
                // skip a corrupted line
            }
        }
        rebuildHeap();
        cout << "Loaded " << waiting.size() << " waiting patient(s) from file.\n";
    }
};

// ============================================================
//  Benchmark: heap vs. plain array scan (evidence for Big-O)
// ============================================================
void benchmark() {
    mt19937 rng(7);
    time_t base = time(nullptr);
    auto makeData = [&](int n) {
        vector<Patient> v((size_t)n);
        for (int i = 0; i < n; i++) {
            v[(size_t)i].id = i + 1;
            v[(size_t)i].age = (int)(rng() % 90 + 1);
            v[(size_t)i].severity = (int)(rng() % 5 + 1);
            v[(size_t)i].arrivalTime = base + i;
        }
        return v;
    };
    auto ms = [](chrono::steady_clock::time_point a) {
        return chrono::duration<double, milli>(chrono::steady_clock::now() - a).count();
    };

    cout << "\nInsert n patients, then treat all of them (time in ms)\n";
    cout << left << setw(12) << "n" << setw(18) << "Heap  O(n log n)" << "Array scan  O(n^2)\n";
    cout << string(50, '-') << "\n";

    for (int n : {1000, 5000, 20000, 200000}) {
        vector<Patient> data = makeData(n);

        auto t0 = chrono::steady_clock::now();
        PatientHeap h;
        for (auto& p : data) h.push(p);
        while (!h.empty()) h.pop();
        double heapMs = ms(t0);

        cout << left << setw(12) << n << setw(18) << fixed << setprecision(2) << heapMs;

        if (n <= 20000) {                               // the array version is too slow beyond this
            t0 = chrono::steady_clock::now();
            vector<Patient> arr;
            for (auto& p : data) arr.push_back(p);
            while (!arr.empty()) {
                size_t best = 0;
                for (size_t i = 1; i < arr.size(); i++)
                    if (higher(arr[i], arr[best])) best = i;
                swap(arr[best], arr.back());
                arr.pop_back();
            }
            cout << ms(t0) << "\n";
        } else {
            cout << "(skipped - too slow)\n";
        }
    }
}

// ============================================================
//  main
// ============================================================
int main() {
    Hospital h;
    h.loadFromFile();

    int choice;
    do {
        cout << "\n========== HOSPITAL EMERGENCY QUEUE ==========\n"
             << " 1. Add Patient\n"
             << " 2. Treat Next Patient\n"
             << " 3. View Waiting List\n"
             << " 4. Search Patient by ID\n"
             << " 5. Update Severity\n"
             << " 6. Daily Report\n"
             << " 7. View Discharged Patients\n"
             << " 8. Toggle Aging (starvation prevention)\n"
             << " 9. Load Patients from sample_patients.txt\n"
             << "10. Generate sample_patients.txt\n"
             << "11. Run Benchmark (heap vs array)\n"
             << " 0. Save & Exit\n";
        choice = readInt("Choice: ", 0, 11);

        switch (choice) {
            case 1:  h.addPatient();      break;
            case 2:  h.treatNext();       break;
            case 3:  h.viewWaiting();     break;
            case 4:  h.searchPatient();   break;
            case 5:  h.updateSeverity();  break;
            case 6:  h.dailyReport();     break;
            case 7:  h.viewDischarged();  break;
            case 8:  h.toggleAging();     break;
            case 9:  h.loadBulk();        break;
            case 10: h.generateSample();  break;
            case 11: benchmark();         break;
            case 0:  cout << "Data saved. Bye!\n"; break;
        }
        h.saveIfDirty();
    } while (choice != 0);

    return 0;
}
