#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <utility>
#include <list>
#include <numeric>
#include <queue>
#include <cassert>
#include <stack>

using namespace std;

class MCC {
private:
  // Task attributes
  struct task_t {
    int id;
    vector<float> lExecTimes;        // local exec. times
    int minlExecTime;              // min(lExecTimes)
    int estimatedRemoteExecTime;
    bool isCloudTask;
    float computationCost;
    float priorityLevel;
    list<int> successorTasks;
    list<int> predecessorTasks;

    task_t(int id_) {
      id = id_;
      minlExecTime = numeric_limits<int>::max();
      priorityLevel = -1.0;
    }
  };

  struct choice_t {
    int task;
    int oldCore;
    int newCore;
  };
  
public:
  MCC(vector<pair<int, int>>&, vector<vector<float>>&, 
		          vector<float>&, float, float);
  void run();
  void printSchedule();

private:
  void primaryAssignment();
  void taskPrioritizing();
  float computePriorityLevel(task_t*);
  void execUnitSelection();
  void taskMigration();
  // 
  void makeGraph(vector<pair<int, int>>&, vector<vector<float>>&);
  pair<float, int> computeFinishTime1(task_t*, vector<float>&);
  float computeFinishTime2(vector<vector<int>>&, task_t*, vector<float>&);
  float computeEnergyCons(vector<vector<int>>&);
  vector<vector<int>> make_S_set(choice_t&);
  pair<float, float> kernelAlgo(choice_t&);
  void taskReschedule(choice_t&);

private:
  vector<task_t*> graph;
  int taskCnt;
  vector<task_t*> pQueue;
  
  // resource attributes
  int coresCnt;
  int sendTime;
  int recvTime;
  int cloudComputeTime;

  float maxCompletionTime;
  vector<float> coresPowerCons;
  float wirelessSendPowerCons;

  // cloud + core schedule
  vector<vector<int>> c_c_sched;
  
  vector<int> wirelessSend_sched;
  vector<int> wirelessRecv_sched;

  // finish times
  vector<float> finishTimes;
};
