#ifndef __TIME_LIST_H__
#define __TIME_LIST_H__
#include "../log/log.h"
#include "timer.h"

class TimerList {
 private:
  Timer* head_;
  Timer* tail_;
  void Insert(Timer* timer, Timer* head);

 public:
  TimerList() : head_(NULL), tail_(NULL) {}
  ~TimerList();
  void Insert(Timer* timer);
  void Adjust(Timer* timer);
  void Delete(Timer* timer);
  void Tick();
};

#endif