#include "http_conn.h"

#include <string.h>

#include "../epoll/epoll.h"
#include "../log/log.h"

void HttpConn::Init() {
  memset(read_buffer_, '\0', kReadBufferSize);
  memset(write_buffer_, '\0', kWriteBufferSize);
  read_index_ = 0;
  write_index_ = 0;
  checked_index_ = 0;
  line_start_index_ = 0;

  http_method_ = NULL;
  http_url_ = NULL;
  http_version_ = NULL;
  host_ = NULL;
  request_body_ = NULL;

  content_length_ = 0;
  is_post_ = false;
  is_keep_alive_ = false;
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
#endif

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

  while ((check_state_ == CHECK_STATE_CONTENT && line_state == LINE_OPEN) ||
         (line_state = ParseLine()) == LINE_OK) {
    text = read_buffer_ + line_start_index_;
    line_start_index_ = checked_index_;
    LOG_INFO("%s", text);
    switch (check_state_) {
      case CHECK_STATE_REQUESTLINE:
        http_code = ParseRequestLine(text);
        if (http_code == BAD_REQUEST) {
          return BAD_REQUEST;
        }
        break;
      case CHECK_STATE_HEADER:
        http_code = ParseHeader(text);
        if (http_code == BAD_REQUEST) {
          return BAD_REQUEST;
        } else if (http_code == GET_REQUEST) {
          return DoRequest();
        }
        break;
      case CHECK_STATE_CONTENT:
        http_code = ParseContent(text);
        if (http_code == BAD_REQUEST) {
          return BAD_REQUEST;
        } else if (http_code == GET_REQUEST) {
          line_state = LINE_OPEN;
          return DoRequest();
        }
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
        read_buffer_[checked_index_++] = '\0';
        read_buffer_[checked_index_++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    } else if (tmp == '\n') {
      if (checked_index_ > 1 && read_buffer_[checked_index_ - 1] == '\r') {
        read_buffer_[checked_index_ - 1] = '\0';
        read_buffer_[checked_index_++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_OPEN;
}

HttpConn::HTTP_CODE HttpConn::ParseRequestLine(char* text) {
  http_url_ = strpbrk(text, " \t");
  if (!http_url_) {
    return BAD_REQUEST;
  }
  *http_url_ = '\0';
  http_url_++;
  http_url_ += strspn(http_url_, " \t");
  if (strcasecmp(http_url_, "GET") == 0) {
    http_method_ = "GET";
    is_post_ = false;
  } else if (strcasecmp(http_method_, "POST") == 0) {
    http_method_ = "POST";
    is_post_ = true;
  } else {
    LOG_WARNING("%s:%ss", "http conn", "only support Get && Post");
    return BAD_REQUEST;
  }

  http_version_ = strpbrk(http_url_, " \t");
  if (!http_version_) {
    LOG_WARNING("%s:%s", "http conn", "none of http version");
  }
  *http_version_ = '\0';
  http_version_++;
  http_version_ += strspn(http_version_, " \t");
  if (strcasecmp(http_version_, "/HTTP1.1") != 0) {
    LOG_WARNING("%s:%s", "http conn", "only support http 1.1");
    return BAD_REQUEST;
  }

  if (strcasecmp(http_url_, "http://") == 0) {
    http_url_ += 7;
    http_url_ = strchr(http_url_, '/');
  }
  if (strcasecmp(http_url_, "https://") == 0) {
    http_url_ += 8;
    http_url_ = strchr(http_url_, '/');
  }
  if (!http_url_ || http_url_[0] != '/') {
    LOG_WARNING("%s:%s", "http conn", "parse url error");
    return BAD_REQUEST;
  }
  if (strlen(http_url_) == 1) {
    strcat(http_url_, "index.html");
  }

  check_state_ = CHECK_STATE_HEADER;
  return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseHeader(char* text) {
  if (text[0] == '\0') {
    if (content_length_ == 0) {
      return GET_REQUEST;
    } else {
      check_state_ = CHECK_STATE_CONTENT;
      return NO_REQUEST;
    }
  } else if (strncasecmp(text, "Connection:", 11) == 0) {
    text += 11;
    text += strspn(text, " \t");
    if (strcasecmp(text, "keep-alive") == 0) {
      is_keep_alive_ = true;
    }
  } else if (strncasecmp(text, "Content-length:", 15) == 0) {
    text += 15;
    text += strspn(text, " \t");
    content_length_ = atol(text);
  } else if (strncasecmp(text, "Host:", 5) == 0) {
    text += 5;
    text += strspn(text, " \t");
    host_ = text;
  } else {
    LOG_WARNING("%s:%s", "http conn", "unkown header");
    return BAD_REQUEST;
  }
  return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseContent(char* text) {
  if (read_index_ >= (content_length_ + checked_index_)) {
    text[content_length_] = '\0';
    request_body_ = text;
    return GET_REQUEST;
  }
  return NO_REQUEST;
}

void HttpConn::Process() { HTTP_CODE http_code = ProcessRead(); }