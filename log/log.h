#ifndef __LOG_H__
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
  char log_name_[MAX_LOG_NAME];    // 日志名称
  char path_name_[MAX_PATH_NAME];  // 日志路径
  int max_buf_size_;               // 最大buffer大小
  char* buf_;                      // buffer
  int max_lines_;                  // 最大行数
  int cur_lines_;                  // 现在的行数
  int day_;                        // 当前日期
  FILE* p_file_;                   // 文件指针
  bool is_async_;                  // 是否异步
  BlockQueue<string>* log_queue_;  // 日志队列
  int max_queue_size_;             // 最大队列长度
  Locker mutex_;                   // 互斥锁

 public:
  // 单例模式
  static Log* GetInstance() {
    static Log instance;
    return &instance;
  }
  // 初始化
  bool Init(char* log_name, int max_queue_size = 0, int max_buf_size = 2560,
            int max_lines = 1000);
  // 写日志
  bool WriteLog(int level, const char* format, ...);

 private:
  // 异步写日志，工作线程
  static void* AsyncWriteProcess(void* args) {
    Log::GetInstance()->AsyncWrite();
  }
  // 异步写日志
  void* AsyncWrite() {
    string single_log;
    while (log_queue_->pop(single_log)) {
      mutex_.lock();
      // 将日志直接放到文件指针中
      fputs(single_log.c_str(), p_file_);
      mutex_.unlock();
    }
  }

  // 同步写日志
  void* SyncWrite(string log_str) {
    mutex_.lock();
    fputs(log_str.c_str(), p_file_);
    mutex_.unlock();
  }
};

// 定义各种LOG类型
#define LOG_DEBUG(format, ...) \
  Log::GetInstance()->WriteLog(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) \
  Log::GetInstance()->WriteLog(1, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) \
  Log::GetInstance()->WriteLog(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) \
  Log::GetInstance()->WriteLog(3, format, ##__VA_ARGS__)

#endif