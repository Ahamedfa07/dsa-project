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
    string name;
    int age;
    int severity;
    time_t arrival Time; 
    string condition;
};
struct compare{
    bool operator()(const patient& a,const patient& b){
        if(a.severity==b.severity)
            return a.arrival>b.arrival;
        
    }

}