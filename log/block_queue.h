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
  BlockQueue(int max_queue_size = 1000) {
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

  bool push(const T& item) {
    mutex_.lock();
    if (cur_queue_size_ >= max_queue_size_) {
      cond_.broadcast();
      mutex_.unlock();
      return false;
    }

    queue_[back_] = item;
    back_ = (back_ + 1) % max_queue_size_;
    cur_queue_size_++;
    mutex_.unlock();
    cond_.broadcast();
    return true;
  }

  bool pop(T& item) {
    mutex_.lock();
    if (cur_queue_size_ <= 0) {
      while (!cond_.wait(mutex_.get())) {
        mutex_.unlock();
        return false;
      }
    }

    item = queue_[front_];
    front_ = (front_ + 1) % max_queue_size_;
    cur_queue_size_--;
    mutex_.unlock();
    return true;
  }

  bool pop(T& item, int ms_timeout) {
    mutex_.lock();
    if (cur_queue_size_ <= 0) {
      struct timespec t = {0, 0};
      struct timeval now = {0, 0};
      gettimeofday(&now, NULL);
      t.tv_sec = now.tv_sec + ms_timeout / 1000;
      t.tv_nsec = (ms_timeout % 1000) * 1000;
      while (!cond_.timewait(mutex_.get(), t)) {
        mutex_.unlock();
        return false;
      }
    }

    if (cur_queue_size_ <= 0) {
      mutex_.unlock();
      return false;
    }

    item = queue_[front_];
    front_ = (front_ + 1) % max_queue_size_;
    cur_queue_size_--;
    mutex_.unlock();
    return true;
  }

  void clear() {
    mutex_.lock() delete[] queue_;
    font_ = back_ = -1;
    cur_queue_size_ = 0;
    mutex_.unlock()
  }

  bool isfull() {
    mutex_.lock();
    if (cur_queue_size_ >= max_queue_size_) {
      mutex_.unlock();
      return true;
    }
    mutex_.unlock();
    return false;
  }

  bool isempty() {
    mutex_.lock();
    if (cur_queue_size_ == 0) {
      mutex_.unlock();
      return true;
    }
    mutex_.unlock();
    return false;
  }

  bool front(T& value) {
    mutex_.lock();
    if (cur_queue_size_ != 0) {
      value = queue_[front_];
      mutex_.unlock();
      return true;
    }
    mutex_.unlock();
    return false;
  }

  bool back(T& value) {
    mutex_.lock();
    if (cur_queue_size_ != 0) {
      value = queue_[back_];
      mutex_.unlock();
      return true;
    }
    mutex_.unlock();
    return false;
  }

  int size() {
    int tmp = -1;
    mutex_.lock();
    tmp = cur_queue_size_;
    mutex_.unlock;
    return tmp;
  }

  int max_size() {
    int tmp = -1;
    mutex_.lock();
    tmp = max_queue_size_;
    mutex_.unlock();
    return tmp;
  }
};
#endif