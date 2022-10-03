#ifndef __LOG__H__
#define __LOG_H__
#include <iostream>

#include "../lock/locker.h"
#include "block_queue.h"
#define MAX_LOG_NAME 256
#define MAX_PATH_NAME 256

using namespace std;

class Log {
 private:
  Log() {
    cur_lines_ = 0;
    is_async_ = false;
  }
  virtual ~Log() {
    if (p_file_) {
      fclose(p_file_);
    }
  }

 private:
  char log_name_[MAX_LOG_NAME];
  char path_name_[MAX_PATH_NAME];
  int max_buf_size_;
  char* buf_;
  int max_lines_;
  int cur_lines_;
  int day_;
  FILE* p_file_;
  bool is_async_;
  BlockQueue<string>* log_queue_;
  int max_queue_size_;
  Locker mutex_;

 public:
  static Log* getinstance() {
    static Log instance;
    return &instance;
  }

  bool init(char* path_name, int max_buf_size = 2560, int max_lines = 1000,
            int max_queue_size = 0);
  bool write_log(int level, const char* format, ...);

 private:
  static void* async_write_process(void* args) {
    Log::getinstance()->async_write();
  }

  void* async_write() {
    string single_log;
    while (log_queue_->pop(single_log)) {
      mutex_.lock();
      fputs(single_log.c_str(), p_file_);
      mutex_.unlock();
    }
  }

  void* sync_write(string log_str) {
    mutex_.lock();
    fputs(log_str.c_str(), p_file_);
    mutex_.unlock();
  }
};

#define LOG_DEBUG(format, ...) \
  Log::getinstance()->write_log(0, format, ##__VA__ARGS__)
#define LOG_INFO(format, ...) \
  Log::getinstance()->write_log(1, format, ##__VA__ARGS__)
#define LOG_WARNING(format, ...) \
  Log::getinstance()->write_log(2, format, ##__VA__ARGS__)
#define LOG_ERROR(format, ...) \
  Log::getinstance()->write_log(3, format, ##__VA__ARGS__)

#endif