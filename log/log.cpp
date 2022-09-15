#include "log.h"

#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "block_queue.h"

bool Log::init(char* log_name, int max_buf_size, int max_lines,
               int max_queue_size) {
  max_buf_size_ = max_buf_size;
  buf_ = new char[max_buf_size_];
  memset(buf_, '\0', max_buf_size_);
  max_lines_ = max_lines;
  max_queue_size_ = max_queue_size;

  // 判断同步还是异步进行日志的写
  if (max_queue_size_ <= 0) {
    is_async_ = false;
    log_queue_ = nullptr;
  } else {
    is_async_ = true;
    log_queue_ = new BlockQueue<std::string>(max_queue_size_);
    pthread_t tid;
    pthread_create(&tid, NULL, async_write, NULL);
  }

  // 获取系统时间
  time_t t = time(NULL);
  struct tm* sys_tm = localtime(&t);
  struct tm my_tm = *sys_tm;

  // 生成log_name
  memset(log_name_, '\0', MAX_LOG_NAME);
  memset(path_name_, '\0', MAX_PATH_NAME);
  const char* p = strrchr(log_name, '/');
  if (p == NULL) {
    strcpy(path_name_, ".");
    strcpy(log_name_, log_name);
  } else {
    strncpy(path_name_, log_name, p - log_name + 1);
    strcpy(log_name_, p + 1);
  }
  char log_full_name[MAX_LOG_NAME] = "\0";
  snprintf(log_full_name, MAX_LOG_NAME, "%d-%02d-%02d-%s", my_tm.tm_year + 1900,
           my_tm.tm_mon + 1, my_tm.tm_mday, log_name_);

  day_ = my_tm.tm_mday;

  p_file_ = fopen(log_full_name, "a");
  if (p_file_ == NULL) {
    return false;
  }
  return true;
}

bool Log::write_log(int level, const char* format, ...) {
  struct timeval now = {0, 0};
  gettimeofday(&now, NULL);
  time_t t = now.tv_sec;
  struct tm* sys_tm = localtime(&t);
  struct tm my_tm = *sys_tm;

  // 写入log level
  char level_str[16] = "\0";
  switch (level) {
    case 0:
      strcpy(level_str, "[debug]");
    case 1:
      strcpy(level_str, "[info]");
    case 2:
      strcpy(level_str, "[warning]");
    case 3:
      strcpy(level_str, "[error]");
  }

  // 写入log time
  mutex_.lock();
  cur_lines_++;
  if (day_ != my_tm.tm_mday || cur_lines_ == max_lines_) {
  } else {
  }
}