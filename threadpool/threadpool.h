#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__
#include <pthread.h>

#include <list>

#include "../db/db_connection_pool.h"
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

template <typename T>
class ThreadPool {
 private:
  Locker mutex_;
  Sem sem_;
  list<T*> request_list_;
  unsigned int kMaxThread;
  unsigned int kMaxRequest;
  ConnectionPool* db_pool_;
  pthread_t* pthread_t_array_;
  bool is_stop_;

 public:
  ThreadPool(ConnectionPool* db_pool, unsigned int max_thread = 8,
             unsigned int max_request = 10000);
  ~ThreadPool() {
    delete[] pthread_t_array_;
    is_stop_ = true;
  }
  static void* Worker(void* arg);
  bool Append(T* request);
  void Run();
};

template <typename T>
ThreadPool<T>::ThreadPool(ConnectionPool* db_pool, unsigned int max_thread,
                          unsigned int max_request) {
  if (max_thread <= 0 || max_request <= 0) {
    LOG_ERROR("%s:%s", "threadpool",
              "max thread and max request must be positive");
    exit(1);
  }
  kMaxRequest = max_request;
  kMaxThread = max_thread;
  pthread_t_array_ = new pthread_t[max_thread];
  for (int i = 0; i < max_thread; i++) {
    if (pthread_create(&pthread_t_array_[i], NULL, Worker, this) != 0) {
      LOG_ERROR("%s:%s", "threadpool", "create worker thread error");
      exit(1);
    }
    if (pthread_detach(pthread_t_array_[i])) {
      LOG_ERROR("%s:%s", "threadpool", "detach thread error");
      exit(1);
    }
  }
}

template <typename T>
void* ThreadPool<T>::Worker(void* arg) {
  ThreadPool* pool = (ThreadPool*)arg;
  pool->Run();
  return pool;
}

template <typename T>
void ThreadPool<T>::Run() {
  while (!is_stop_) {
    sem_.wait();
    mutex_.lock();
    if (request_list_.empty()) {
      continue;
      mutex_.unlock();
    }
    T* request = request_list_.front();
    request_list_.pop_front();
    mutex_.unlock();
    if (!request) continue;
    ConnectionRAII mysqlconn(&request->conn_, db_pool_);
    request->Process();
  }
}

template <typename T>
bool ThreadPool<T>::Append(T* request) {
  if (request_list_.size() < kMaxRequest) {
    mutex_.lock();
    request_list_.push_back(request);
    mutex_.unlock();
    sem_.post();
    return true;
  }
  LOG_WARNING("%s:%s", "threadpool", "too many requests,threadpool busy");
  return false;
}

#endif