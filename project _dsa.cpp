#include <iostream>
#include <queue>
#include <vector>
#include <unordered_map>
#include <string>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <algorithm>
using namespace std;

struct patient{
    int id;
    string  name;
    int age;
    int severity;
    time_t arrival Time; 
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

class hospital{
private:
    priority_queue<Patient, vector<Patient>, Compare> pq;
    unordered_map<int, Patient> patientMap;
    queue<Patient> dischargeLog;
    vector<double> waitTimes;
    vector<int> treatedSeverity;
    int nextId = 1001;

public:
    void addPatient() {
        Patient p;
        p.id = nextId++;
        cin.ignore();
        cout << "Name: ";            getline(cin, p.name);
        cout << "Age: ";             cin >> p.age;
        cin.ignore();
        cout << "Condition: ";       getline(cin, p.condition);
        do {
            cout << "Severity (1-5): ";
            cin >> p.severity;
        } while (p.severity < 1 || p.severity > 5);
        p.arrivalTime = time(nullptr);

        pq.push(p);
        patientMap[p.id] = p;
        cout << "Patient added. ID = " << p.id << "\n";
    }

    void treatNext() {
        if (pq.empty()) { cout << "No patients waiting.\n"; return; }
        Patient p = pq.top();
        pq.pop();
        patientMap.erase(p.id);

        double wait = difftime(time(nullptr), p.arrivalTime);
        waitTimes.push_back(wait);
        treatedSeverity.push_back(p.severity);
        dischargeLog.push(p);

        cout << "Treating: " << p.name << " (ID " << p.id
             << ", Severity " << p.severity << ")\n";
        cout << "Waited " << wait << " seconds\n";
    }
};
