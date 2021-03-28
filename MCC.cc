#include <MCC.h>

MCC::MCC(vector<pair<int, int>>& G, vector<vector<float>>& lExecTimes,
                                    vector<float>& coresPowerCons_,
		                    float sendPowerCons, float maxTime) {
  makeGraph(G, lExecTimes);
  taskCnt = graph.size();
  coresCnt = coresPowerCons_.size();
  // TODO: should not be hardcoded like this
  sendTime = 3;
  recvTime = 1;
  cloudComputeTime = 1;

  maxCompletionTime = maxTime;
  coresPowerCons = coresPowerCons_;
  wirelessSendPowerCons = sendPowerCons;

  c_c_sched = vector<vector<int>> (coresCnt+1, vector<int>{});
}

void
MCC::run() {
  // Step 1
  primaryAssignment();
  taskPrioritizing();
  execUnitSelection();

  // print the schedule
  cout << "===== INITIAL SCHEDULE =====" << endl;
  printSchedule();
   
  // Step 2
  taskMigration();

  // print the schedule
  cout << endl << "===== MCC TASK SCHEDULE =====" << endl;
  printSchedule();
}

void
MCC::makeGraph(vector<pair<int, int>>& G, vector<vector<float>>& lExecTimes) {
  int sz = 0;
  for (auto e : G) {
    if (e.first > sz)
      sz = e.first;
    if (e.second > sz)
      sz = e.second;
  }

  // To allow the graph to be 1 indexed
  sz = sz + 1;

  graph = vector<task_t*>(sz, NULL);
  for (int i=0; i<G.size(); ++i) {
    if (graph[G[i].first] == NULL)
      graph[G[i].first] = new task_t(G[i].first);
    (graph[G[i].first]->successorTasks).push_back(G[i].second);
    if (graph[G[i].second] == NULL)
      graph[G[i].second] = new task_t(G[i].second);
    (graph[G[i].second]->predecessorTasks).push_back(G[i].first);
  }

  for (auto lExec : lExecTimes) {
    graph[lExec[0]]->lExecTimes = lExec;
  }
}

void 
MCC::primaryAssignment() {
  // Calculate the minimum local exec. time
  for (int task = 1; task<graph.size(); ++task) {
    graph[task]->minlExecTime = *min_element(graph[task]->lExecTimes.begin()+1, 
		                     graph[task]->lExecTimes.end());
  }

  // Calculate the estimated remote exec. time
  for (int task = 1; task<graph.size(); ++task) {
  // for (auto node : graph) {
    graph[task]->estimatedRemoteExecTime = sendTime + 
	                                   recvTime + cloudComputeTime;
    if (graph[task]->estimatedRemoteExecTime < graph[task]->minlExecTime)
      graph[task]->isCloudTask = true;
  }
}

void 
MCC::taskPrioritizing() {
  for (int task=1; task<graph.size(); ++task) {
    if (graph[task]->isCloudTask)
      graph[task]->computationCost = graph[task]->estimatedRemoteExecTime;
    else {
      size_t sz = graph[task]->lExecTimes.size();
      graph[task]->computationCost = accumulate(graph[task]->lExecTimes.begin()+1, 
		                         graph[task]->lExecTimes.end(), 0.0)/sz;
    }
  }

  // Calculate the priority levels
  for (int task=1; task<graph.size(); ++task) {
    computePriorityLevel(graph[task]);
  }

  // Populate the priority queue
  auto cmp = [] (task_t* a, task_t* b) {
    return a->priorityLevel < b->priorityLevel;};
  for (int task=1; task<graph.size(); ++task)
    pQueue.push_back(graph[task]);
    sort(pQueue.begin(), pQueue.end(), cmp);
}

float 
MCC::computePriorityLevel(task_t* task) {
  if (task->priorityLevel != -1.0)
    return task->priorityLevel;
  if (task->successorTasks.empty()) {
    task->priorityLevel = task->computationCost;
    return task->priorityLevel;
  }

  float priority = 0.0;
  for (auto task : task->successorTasks) {
    if (priority < computePriorityLevel(graph[task]))
      priority = graph[task]->priorityLevel;
  }
  task->priorityLevel = task->computationCost + priority;

  return task->priorityLevel;
}

void 
MCC::execUnitSelection() {
  vector<float> finish_times(graph.size(), -1);
  for (int i=pQueue.size()-1; i>=0; --i) {
    task_t* task = pQueue[i];
    pair<float, int> result = computeFinishTime1(task, finish_times);
    if (result.second == 0) {
      task->isCloudTask = true;
    }
    c_c_sched[result.second].push_back(task->id);
    finish_times[task->id] = result.first;
  }
  finishTimes = finish_times;
}

pair<float, int>
MCC::computeFinishTime1(task_t* task, vector<float>& finish_times) {
  float finish_pred = 0;
  float finish_sched = 0;
  float finish_time = 0;
  float cloud_finish_time = 0;
  float min_finish_time = numeric_limits<float>::max();
  int which_core = 0;

  // task-precedence
  for (auto it = graph[task->id]->predecessorTasks.begin();
	    it != graph[task->id]->predecessorTasks.end(); ++it) {
    if (finish_times[*it] > finish_pred)
      finish_pred = finish_times[*it];
  }
  
  // if 'task' is a cloud task
  if (task->isCloudTask) {
    if (c_c_sched[0].empty()) {
      cloud_finish_time = finish_pred + sendTime + 
	                  cloudComputeTime + recvTime;
      return {cloud_finish_time, 0};
    }
    else {
      float cloud_send_time = finish_times[c_c_sched[0].back()] - 
      	              (cloudComputeTime + recvTime);
      if (cloud_send_time > finish_pred) {
        cloud_finish_time = cloud_send_time + sendTime + 
      	              cloudComputeTime + recvTime;
        return {cloud_finish_time, 0};
      }
      else {
        cloud_finish_time = finish_pred + sendTime + 
      	              cloudComputeTime + recvTime;
        return {cloud_finish_time, 0};
      }
    }
  }

  // searching for a core which would yield the least finish time
  // 1. if it were scheduled on the cloud
  if (c_c_sched[0].empty()) {
    cloud_finish_time = finish_pred + sendTime + 
                        cloudComputeTime + recvTime;
  }
  else {
    float cloud_send_time = finish_times[c_c_sched[0].back()] - 
    	              (cloudComputeTime + recvTime);
    if (cloud_send_time > finish_pred) {
      cloud_finish_time = cloud_send_time + sendTime + 
    	              cloudComputeTime + recvTime;
    }
    else {
      cloud_finish_time = finish_pred + sendTime + 
    	              cloudComputeTime + recvTime;
    }
  }
  // 2. if it were scheduled on one of the cores
  float finish_core = numeric_limits<float>::max();
  for (int i=1; i<coresCnt+1; ++i) {
    float f_time = 0;
    if (c_c_sched[i].empty())
      f_time = finish_pred + task->lExecTimes[i];
    else {
      if (finish_pred > finish_times[c_c_sched[i].back()])
        f_time = finish_pred + task->lExecTimes[i];
      else
	f_time = finish_times[c_c_sched[i].back()] + task->lExecTimes[i];
    }
    if (f_time < finish_core) {
      finish_core = f_time;
      which_core = i;
    }
  } 
  
  if (cloud_finish_time < finish_core)
    return {cloud_finish_time, 0};
  else
    return {finish_core, which_core};
}


float
MCC::computeFinishTime2(vector<vector<int>>& S,
		        task_t* task, vector<float>& finish_times) {
  float finish_pred = 0;
  float cloud_send_time = 0;
  unordered_map<int, pair<int, int>> hash;
  for (int i=0; i<S.size(); ++i) {
    for (int j=0; j<S[i].size(); ++j) {
      hash[S[i][j]] = {i, j};
    }
  }

  int core = hash[task->id].first;
  // task precedence for a cloud task
  for (auto it = graph[task->id]->predecessorTasks.begin();
	    it != graph[task->id]->predecessorTasks.end(); ++it) {
    if (finish_times[*it] > finish_pred) {
      if (hash[*it].first == 0 && core == 0)
        finish_pred = finish_times[*it] - (cloudComputeTime + recvTime);
      else
	finish_pred = finish_times[*it];
    }
  }

  // if it's a cloud task
  if (core == 0) {
    if (S[0].empty())
      cloud_send_time = 0;
    else {
      if (hash[task->id].second == 0)
        cloud_send_time = 0;
      else {
	int idx = hash[task->id].second-1;
        cloud_send_time = finish_times[S[0][idx]]- 
	                  (cloudComputeTime + recvTime);
      }
    }
    if (cloud_send_time > finish_pred)
      return cloud_send_time + sendTime + cloudComputeTime + recvTime;
    else
      return finish_pred + sendTime + cloudComputeTime + recvTime;
  }

  // task-precedence
  for (auto it = graph[task->id]->predecessorTasks.begin();
	    it != graph[task->id]->predecessorTasks.end(); ++it) {
    if (finish_times[*it] > finish_pred) {
      finish_pred = finish_times[*it];
    }
  }

  // tasks scheduled on the same core
  float finish_time = 0;
  for (int j=1; j<S[core].size(); ++j) {
    if (S[core][j] == task->id)
      break;
    if (finish_times[S[core][j]] > finish_time)
      finish_time = finish_times[S[core][j]];
  }
  if (finish_time > finish_pred)
    return finish_time + task->lExecTimes[core];
  else
    return finish_pred + task->lExecTimes[core];
}

// TODO:
void 
MCC::taskMigration() {
  while (1) {
    vector<choice_t> choices;
    choice_t theChoice = {0, 0, 0};

    float prevEtotal;
    float prevTtotal;
    pair<float, float> result = kernelAlgo(theChoice);
    prevEtotal = result.first;
    prevTtotal = result.second;

    // the "N'*K" choices
    for (int i=1; i<coresCnt+1; ++i) {
      for (int j=0; j<c_c_sched[i].size(); ++j) {
	for (int k=0; k<coresCnt+1; ++k) {
          if (k == i)
            continue;
	  choices.push_back({c_c_sched[i][j], i, k});
	}
      }
    }

    // choose the 'choice' with the max Etotal reduction
    float Ereduction = 0.0;
    for (auto choice : choices) {
      result = kernelAlgo(choice);
      if (result.second > prevTtotal)
        continue;
      if ((prevEtotal - result.first) > Ereduction) {
        theChoice = choice;
        Ereduction = prevEtotal - result.first;
      }
    }

    // choose the 'choice' with the max 'ratio'
    float ratio = 0.0;
    if (Ereduction == 0) {
      for (auto choice : choices) {
        result = kernelAlgo(choice);
        if (result.second > maxCompletionTime)
          continue;

        float new_ratio = (prevEtotal - result.first) / 
                          (result.second - prevTtotal);
        if (new_ratio > ratio && result.first < prevEtotal) {
          theChoice = choice;
          ratio = new_ratio;
        }
      }
    }
    if (ratio == 0.0 && Ereduction == 0.0) { // Etotal is not changing
      // To update 'finishTimes'
      result = kernelAlgo(theChoice);
      break;
    }
    
    result = kernelAlgo(theChoice);
    c_c_sched = make_S_set(theChoice);
    if (theChoice.newCore == 0)
      graph[theChoice.task]->isCloudTask = true;
  }
}


vector<vector<int>>
MCC::make_S_set(choice_t& choice) {
  vector<vector<int>> S(coresCnt+1, vector<int>{});
  for (int i=0; i<coresCnt+1; ++i) {
    if (i == choice.newCore)
      continue;
    if (i == choice.oldCore) {
      for (int j = 0; j<c_c_sched[choice.oldCore].size(); ++j) {
        if (c_c_sched[i][j] != choice.task)
          S[i].push_back(c_c_sched[i][j]);
      }
      continue;
    }
    S[i] = c_c_sched[i];
  }

  // Task precedence
  for (int j=0; j<c_c_sched[choice.newCore].size(); ++j) {
    int task = c_c_sched[choice.newCore][j];
    if ((graph[task]->priorityLevel >= graph[choice.task]->priorityLevel) &&
        task != choice.task)
      S[choice.newCore].push_back(task);
  }
  S[choice.newCore].push_back(choice.task);
  for (int j=0; j<c_c_sched[choice.newCore].size(); ++j) {
    int task = c_c_sched[choice.newCore][j];
    if (graph[task]->priorityLevel < graph[choice.task]->priorityLevel)
      S[choice.newCore].push_back(task);
  }

  return S;
}

pair<float, float>
MCC::kernelAlgo(choice_t& choice) {
  vector<vector<int>> new_S;
  if (choice.task == 0)
    new_S = c_c_sched;
  else
    new_S = make_S_set(choice);

  // ready1 and ready2 size is actualy taskCnt+1, to allow 1 indexing
  vector<int> ready1(taskCnt, 0);
  vector<int> ready2(taskCnt, 0);

  for (int i=1; i<graph.size(); ++i)
    ready1[i] = graph[i]->predecessorTasks.size();

  for (int i=0; i<new_S.size(); ++i)
    for (int j=0; j<new_S[i].size(); ++j)
        ready2[new_S[i][j]] = j;
 
  // <taskID, <core, j>> hash
  unordered_map<int, pair<int, int>> hash;
  for (int i=0; i<new_S.size(); ++i) 
    for (int j=0; j<new_S[i].size(); ++j)
      hash[new_S[i][j]] = {i, j};
   
  stack<int> theStack;
  for (int i=1; i<ready1.size(); ++i) {
    if (ready1[i] == 0 && ready2[i] == 0) {
      theStack.push(i);
      ready1[i] = -1;
      ready2[i] = -1;
    }
  }
  
  vector<float> finish_times(taskCnt, -1);
  while (!theStack.empty()) {
    int taskId;
    float startTime;
    float finishTime;

    taskId = theStack.top(); theStack.pop();
    finishTime = computeFinishTime2(new_S, graph[taskId], finish_times);
    finish_times[taskId] = finishTime;

    for (auto it = graph[taskId]->successorTasks.begin();
	      it != graph[taskId]->successorTasks.end(); ++it) {
      ready1[*it] -= 1;
    }
    for (int j=hash[taskId].second+1; 
             j<new_S[hash[taskId].first].size(); ++j) {
      int idx = new_S[hash[taskId].first][j];
      ready2[idx] -= 1;
    }

    // update 'theStack'
    for (int i=1; i<ready1.size(); ++i) {
      if (ready1[i] == 0 && ready2[i] == 0) {
        theStack.push(i);
        ready1[i] = -1;
	ready2[i] = -1;
      }
    }
  }

  float finish_time = *max_element(finish_times.begin(), finish_times.end());
  float energyCons = computeEnergyCons(new_S);

  finishTimes = finish_times;
  return {energyCons, finish_time};
}

float
MCC::computeEnergyCons(vector<vector<int>>& S) {
  float Etotal = 0.0;

  for (int i=0; i<S[0].size(); ++i)
    Etotal += wirelessSendPowerCons * sendTime;
  
  for (int i=1; i<S.size(); ++i) {
    for (int j=0; j<S[i].size(); ++j) {
      float lExTime = graph[S[i][j]]->lExecTimes[i];
      Etotal += coresPowerCons[i-1] * lExTime;
    }
  }

  return Etotal;
}


void
MCC::printSchedule() {

    float energyCons = 0;
    
    unordered_map<int, int> hash;
    for(int core = 1; core <= coresCnt ;++core) {
        for(int j = 0; j < c_c_sched[core].size(); ++j)
            hash[c_c_sched[core][j]] = core;
    }
            
  
  for (int task = 1; task < graph.size(); ++task) {
    float cloud_finishTime = finishTimes[task] - recvTime;
    float send_finishTime = cloud_finishTime - cloudComputeTime;
    float core_startTime = finishTimes[task] - 
	                   graph[task]->lExecTimes[hash[task]]; 
    
    cout << "Task " << graph[task]->id << ": ";
    if (graph[task]->isCloudTask ==true) {
      float cloud_energyCons = 0.5*sendTime;
      energyCons += cloud_energyCons;
      cout << "Cloud Task | " 
      << "Wireless Sending: " << send_finishTime - sendTime 
                              << " - " << send_finishTime << " | "
      << "Cloud: " << send_finishTime << " - " << cloud_finishTime << " | "
      << "Wireless Receiving: " << cloud_finishTime << " - " 
                                << finishTimes[task] << " | " << endl;
      }
    else {
      float core_energyCons = coresPowerCons[hash[task]-1] * 
	                      graph[task]->lExecTimes[hash[task]];
      energyCons += core_energyCons;
      cout << "Core Task | "
            << "Core #" << hash[task] << " | "
      << "Time: " << core_startTime << " - " 
                  << finishTimes[task] << " | " << endl;
    }
    cout << "Finish Time: " << finishTimes[task] << endl;
  }
  cout << endl << "Application Total Energy Consumption: " << energyCons << endl
       << "Application Runtime: " << *max_element(finishTimes.begin(), finishTimes.end()) 
                      << endl;
}
