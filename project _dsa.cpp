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
priority_queue<Patient, vector<Patient>, Compare> pq;
unordered_map<int, Patient> patientMap;
queue<Patient> dischargeLog;
vector<double> waitTimes;
vector<int> treatedSeverity;
int nextId = 1001;