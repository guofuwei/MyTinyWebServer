#ifndef __LOCK_H__
#define __LOCK_H__

#include <exception>

#include "pthread.h"

class Locker {
 private:
  pthread_mutex_t mutex_lock_;

 public:
  Locker() {
    if (pthread_mutex_init(&mutex_lock_, NULL) != 0) {
      throw std::exception();
    }
  }
  ~Locker() { pthread_mutex_destroy(&mutex_lock_); }
  bool lock() { return pthread_mutex_lock(&mutex_lock_) == 0; }
  bool unlock() { return pthread_mutex_unlock(&mutex_lock_) == 0; }
  pthread_mutex_t* get() { return &mutex_lock_; }
};

class Cond {
 private:
  pthread_cond_t cond_;

 public:
  Cond() {
    if (pthread_cond_init(&cond_, NULL) != 0) {
      throw std::exception();
    }
  }
  ~Cond() { pthread_cond_destroy(&cond_); }
  bool wait(pthread_mutex_t* p_mutex_lock) {
    return pthread_cond_wait(&cond_, p_mutex_lock) == 0;
  }
  bool timewait(pthread_mutex_t* p_mutex_lock, struct timespec* p_t) {
    return pthread_cond_timedwait(&cond_, p_mutex_lock, p_t) == 0;
  }
  bool signal() { return pthread_cond_signal(&cond_) == 0; }
  bool broadcast() { return pthread_cond_broadcast(&cond_) == 0; }
};

#endif