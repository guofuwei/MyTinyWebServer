#include "http_conn.h"

#include "../epoll/epoll.h"
#include "../log/log.h"
#include "string.h"

void HttpConn::Init() {
  memset(read_buffer_, '\0', kReadBufferSize);
  memset(write_buffer_, '\0', kWriteBufferSize);
  read_index_ = 0;
  write_index_ = 0;
  checked_index_ = 0;
  line_start_index_ = 0;
}

void HttpConn::Init(sockaddr_in sockaddr, int sockfd) {
  Init();
  sockaddr_ = sockaddr;
  sockfd_ = sockfd;
  AddFd(epoll_fd_, sockfd, true);
  conn_count_++;
}

bool HttpConn::Read() {
  if (read_index_ >= kReadBufferSize) {
    LOG_WARNING("%s:%s", "HttpConn", "read index overflow");
    return false;
  }
  int bytes_read = 0;
#ifdef CONNFDLT
  bytes_read = recv(sockfd_, read_buffer_ + read_index_,
                    kReadBufferSize - read_index_, 0);
  if (bytes_read <= 0) return false;
  read_index_ += bytes_read;
  return true;
#endif  // DEBUG

#ifdef CONNFDET
  while (true) {
    bytes_read = recv(sockfd_, read_buffer_ + read_index_,
                      kReadBufferSize - read_index_, 0);
    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      return false;
    } else if (bytes_read == 0) {
      return false;
    }
    read_index_ += bytes_read;
  }
  return true;
#endif
}

HttpConn::HTTP_CODE HttpConn::ProcessRead() {
  LINE_STATE line_state = LINE_OK;
  HTTP_CODE http_code = NO_REQUEST;
  char* text;

  while (line_state == LINE_OK ||
         (check_state_ == CHECK_STATE_CONTENT && line_state == LINE_OPEN)) {
    text = read_buffer_ + line_start_index_;
    line_start_index_ = checked_index_;
    LOG_INFO("%s", text);
    switch (check_state_) {
      case CHECK_STATE_REQUESTLINE:
        break;
      case CHECK_STATE_HEADER:
        break;
      case CHECK_STATE_CONTENT:
        break;
      default:
        return INTERNAL_ERROR;
    }
  }
}

HttpConn::LINE_STATE HttpConn::ParseLine() {
  char tmp;
  for (; checked_index_ <= read_index_; checked_index_++) {
    tmp = read_buffer_[checked_index_];
    if (tmp == '\r') {
      if ((checked_index_ + 1) == read_index_)
        return LINE_OPEN;
      else if (read_buffer_[checked_index_ + 1] == '\n') {
        read_buffer_[checked_index_] = '\0';
        read_buffer_[checked_index_ + 1] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    } else if (tmp == '\n') {
      if (checked_index_ > 1 && read_buffer_[checked_index_ - 1] == '\r') {
        read_buffer_[checked_index_ - 1] = '\0';
        read_buffer_[checked_index_] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_OPEN;
}

void HttpConn::Process() { HTTP_CODE http_code = ProcessRead(); }