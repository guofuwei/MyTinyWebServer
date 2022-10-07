#ifndef __TIMER_H__
#define __TIMER_H__
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>

class Timer;
struct ClientData {
  sockaddr_in sockaddr_;
  int sockfd_;
  Timer timer;
};

class Timer {
 public:
  Timer() : prev_(NULL), next_(NULL){};

  time_t expire_time_;
  void (*callback_func_)(ClientData*);
  ClientData* client_data_;
  Timer* prev_;
  Timer* next_;
};

#endif