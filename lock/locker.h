#ifndef __LOCK_H__
#define __LOCK_H__

#include <semaphore.h>

#include <exception>

#include "pthread.h"

class Sem {
 private:
  sem_t sem_;

 public:
  Sem() {
    if (sem_init(&sem_, 0, 0) != 0) {
      throw std::exception();
    }
  }
  Sem(int num) {
    if (sem_init(&sem_, 0, num) != 0) {
      throw std::exception();
    }
  }
  ~Sem() { sem_destroy(&sem_); }
  bool wait() { return sem_wait(&sem_) == 0; }
  bool post() { return sem_post(&sem_) == 0; }
};

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