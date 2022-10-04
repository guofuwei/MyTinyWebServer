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
  static Log* GetInstance() {
    static Log instance;
    return &instance;
  }

  bool Init(char* path_name, int max_buf_size = 2560, int max_lines = 1000,
            int max_queue_size = 0);
  bool WriteLog(int level, const char* format, ...);

 private:
  static void* AsyncWriteProcess(void* args) {
    Log::GetInstance()->AsyncWrite();
  }

  void* AsyncWrite() {
    string single_log;
    while (log_queue_->pop(single_log)) {
      mutex_.lock();
      fputs(single_log.c_str(), p_file_);
      mutex_.unlock();
    }
  }

  void* SyncWrite(string log_str) {
    mutex_.lock();
    fputs(log_str.c_str(), p_file_);
    mutex_.unlock();
  }
};

#define LOG_DEBUG(format, ...) \
  Log::GetInstance()->WriteLog(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) \
  Log::GetInstance()->WriteLog(1, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) \
  Log::GetInstance()->WriteLog(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) \
  Log::GetInstance()->WriteLog(3, format, ##__VA_ARGS__)

#endif