#include <MCC.h>

double CLOCK();

int main() {
  // graph 1.
  vector<pair<int, int>> G = {
    {1, 2}, {2, 8}, {3, 7}, {4, 8},
    {1, 3}, {2, 9}, {4, 9}, {1, 4}, 
    {1, 5}, {1, 6}, {5, 9}, {6, 8}, 
    {7, 10}, {8, 10}, {9, 10}
  };
  
  // graph 2.
  // vector<pair<int, int>> G = {
  //   {1, 3}, {1, 4}, {2, 4}, {2, 5},
  //   {3, 6}, {4, 6}, {4, 7}, {5, 7},
  //   {5, 9}, {6, 8}, {7, 8}, {8, 9},
  //   {9, 10}
  // };

  // graph 3.
  // vector<pair<int, int>> G = {
  //   {1, 3}, {1, 4}, {1, 5}, 
  //   {2, 3}, {2, 4}, {2, 5},
  //   {3, 6}, {4, 6}, {5, 6},
  //   {6, 7}, {6, 8}, {6, 9},
  //   {9, 10}
  // };


  vector<vector<float>> localExecTimes = {
    {1, 9, 7, 5},
    {2, 8, 6, 5},
    {3, 6, 5, 4},
    {4, 7, 5, 3},
    {5, 5, 4, 2},
    {6, 7, 6, 4},
    {7, 8, 5, 3},
    {8, 6, 4, 2},
    {9, 5, 3, 2},
    {10, 7, 4, 2}
  };

  vector<float> coresPowerCons = {1, 2, 4};
  float powerConsWirelessSend = 0.5; 
  float maxCompletionTime = 27;

  MCC mcc = MCC(G, localExecTimes, coresPowerCons, powerConsWirelessSend, 
		                                       maxCompletionTime);
  double startTime = CLOCK();
  mcc.run();
  
  cout << endl << endl << "Runtime (ms): " << CLOCK() - startTime << endl;

  return 0;
}

double CLOCK() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC,  &t);
  return (t.tv_sec * 1000)+(t.tv_nsec*1e-6);
}
