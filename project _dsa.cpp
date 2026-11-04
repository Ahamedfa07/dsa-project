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
struct compare{
    bool operator()(const patient& a,const patient& b){
        if(a.severity==b.severity)
            return a.arrival>b.arrival;
        return a.severity<b.severity;
        
    }

};

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
