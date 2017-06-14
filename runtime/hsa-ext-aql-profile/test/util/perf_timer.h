#ifndef _PERF_TIMER_H_
#define _PERF_TIMER_H_

// Will use AMD timer and general Linux timer based on users' need --> compilation flag
// need to consider platform is Windows or Linux

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <string>
#if defined(_MSC_VER)
#include <time.h>
#include <windows.h>
#include <intrin.h>
#else
#if defined(__GNUC__)
#include <sys/time.h>
#include <x86intrin.h>
#endif  // __GNUC__
#endif  //_MSC_VER

using namespace std;

class PerfTimer {
 public:
  enum { SUCCESS = 0, FAILURE = 1 };

  PerfTimer();
  ~PerfTimer();

  // General Linux timing method
  int CreateTimer();
  int StartTimer(int index);
  int StopTimer(int index);

  // retrieve time
  double ReadTimer(int index);
  // write into a file
  double WriteTimer(int index);

 private:
  struct Timer {
    string name;     /* < name name of time object*/
    long long _freq; /* < _freq frequency*/
    double _clocks;  /* < _clocks number of ticks at end*/
    double _start;   /* < _start start point ticks*/
  };

  std::vector<Timer*> _timers; /*< _timers vector to Timer objects */
  double freq_in_100mhz;

  // AMD timing method
  uint64_t CoarseTimestampUs();
  uint64_t MeasureTSCFreqHz();

  void Error(string str);
};

#endif  // _PERF_TIMER_H_
