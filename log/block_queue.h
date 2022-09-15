#ifndef __BLOCK_QUEUE_H__
#define __BLOCK_QUEUE_H__

#include <string>

#include "../lock/locker.h"

using namespace std;

template <typename T>
class BlockQueue {
 private:
  int max_queue_size_;
  int cur_queue_size_;
  T* queue_;
  int front_;
  int back_;
  Locker mutex_;
  Cond cond_;

 public:
  BlockQueue(int max_queue_size) {
    max_queue_size_ = max_queue_size;
    queue_ = new T[max_queue_size_];
    cur_queue_size_ = 0;
    front_ = back_ = -1;
  }
  ~BlockQueue() {
    mutex_.lock();
    delete[] queue_;
    mutex_.unlock();
  }

  bool push(const T& log_str) {
    mutex_.lock();
    if (cur_queue_size_ >= max_queue_size_) {
      cond_.broadcast();
      mutex_.unlock();
      return false;
    }
    back_ = (back_ + 1) % max_queue_size_;
    queue_[back_] = log_str;
    cur_queue_size_++;

    mutex_.unlock();
    cond_.broadcast();
    return true;
  }

  bool pop(T& log_str) {
    mutex_.lock();
    if (cur_queue_size_ <= 0) {
      while (!cond_.wait(mutex_.get())) {
        mutex_.unlock();
        return false;
      }
    }
    front_ = (front_ + 1) % max_queue_size_;
    log_str = queue_[front_];
    cur_queue_size_--;

    mutex_.unlock();
    return true;
  }
};
#endif