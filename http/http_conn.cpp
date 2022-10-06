#include "http_conn.h"

#include <string.h>

#include <map>

#include "../epoll/epoll.h"
#include "../lock/locker.h"
#include "../log/log.h"

map<string, string> USERS;
Locker USERLOCK;
const char* ROOTFILE = "/home/hanshan/MyTinyWebServer";

//定义http响应的一些状态信息
const char* STATUS200 = "OK";
const char* STATUS400 = "Bad Request";
const char* STATUS403 = "Forbidden";
const char* STATUS404 = "Not Found";
const char* STATUS500 = "Internal Error";

const char* CONTENT400 =
    "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* CONTENT403 =
    "You do not have permission to get file form this server.\n";
const char* CONTENT404 = "The requested file was not found on this server.\n";
const char* CONTENT500 =
    "There was an unusual problem serving the request file.\n";
const char* CONTENT_EMPTY = "<html><body></body></html>";

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

  memset(real_file_, '\0', kFileNameLen);
  file_address_ = NULL;

  write_vi_count_ = 0;
  bytes_send_ = 0;
  bytes_have_send_ = 0;
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
    // LOG_WARNING("%s:%s", "http conn", "unkown header");
    // return BAD_REQUEST;
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

HttpConn::HTTP_CODE HttpConn::DoRequest() {
  strcpy(real_file_, ROOTFILE);

  const char* p = strrchr(http_url_, '/');
  char* url_real = (char*)malloc(sizeof(char) * 200);

  if (is_post_ && (*(p + 1) == '2' || *(p + 1) == '3')) {
    char name[100];
    char password[100];
    int i = 0;
    for (i = 5; request_body_[i] != '&'; i++) name[i - 5] = request_body_[i];
    name[i - 5] = '\0';
    int j = 0;
    for (i = i + 10; request_body_[i] != '\0'; ++i, ++j)
      password[j] = request_body_[i];
    password[j] = '\0';
    if (*(p + 1) == '3') {
      //如果是注册，先检测数据库中是否有重名的
      //没有重名的，进行增加数据
      char* sql_insert = (char*)malloc(sizeof(char) * 200);
      strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
      strcat(sql_insert, "'");
      strcat(sql_insert, name);
      strcat(sql_insert, "', '");
      strcat(sql_insert, password);
      strcat(sql_insert, "')");

      if (USERS.find(name) != USERS.end()) {
        strcpy(http_url_, "/registerError.html");
      } else {
        USERLOCK.lock();
        int res = mysql_query(conn_, sql_insert);
        USERS.insert(pair<string, string>(name, password));
        USERLOCK.unlock();

        if (!res) {
          strcpy(url_real, "/log.html");
        } else {
          strcpy(url_real, "/registerError.html");
        }
      }
    } else if (*(p + 1) == '2') {
      if (USERS.find(name) != USERS.end() &&
          USERS.find(password) != USERS.end()) {
        strcpy(url_real, "/welcome.html");
      } else {
        strcpy(url_real, "/logError.html");
      }
    }
  } else if (*(p + 1) == '0') {
    strcpy(url_real, "/register.html");
  } else if (*(p + 1) == '1') {
    strcpy(url_real, "/log.html");
  } else if (*(p + 1) == '5') {
    strcpy(url_real, "/picture.html");
  } else if (*(p + 1) == '6') {
    strcpy(url_real, "/video.html");
  } else if (*(p + 1) == '7') {
    strcpy(url_real, "/fans.html");
  } else {
    strcpy(url_real, "404.html");
  }

  if ((strlen(ROOTFILE) + strlen(url_real)) > kFileNameLen) {
    LOG_WARNING("%s:%s", "http conn", "too long request url");
    free(url_real);
    return BAD_REQUEST;
  }
  strcat(real_file_, url_real);
  free(url_real);

  if (stat(real_file_, &file_stat_) < 0) return NO_RESOURCE;
  if (!(file_stat_.st_mode & S_IROTH)) return FORBIDDEN_REQUEST;
  if (S_ISDIR(file_stat_.st_mode)) return BAD_REQUEST;
  int fd = open(real_file_, O_RDONLY);
  file_address_ =
      (char*)mmap(0, file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  return FILE_REQUEST;
}

void HttpConn::Process() {
  HTTP_CODE read_code = ProcessRead();
  if (read_code == NO_REQUEST) {
    ModFd(epoll_fd_, sockfd_, EPOLLIN);
    return;
  }
  bool write_ret = ProcessWrite(read_code);
  if (!write_ret) {
    CloseConn();
  }
  ModFd(epoll_fd_, sockfd_, EPOLLOUT);
}

void HttpConn::CloseConn() {
  if (sockfd_ != -1) {
    RemoveFd(epoll_fd_, sockfd_);
    conn_count_--;
    sockfd_ = -1;
  }
}

bool HttpConn::ProcessWrite(HTTP_CODE read_code) {
  switch (read_code) {
    case INTERNAL_ERROR:
      AddStatusLine(500, STATUS500);
      AddHeader(strlen(CONTENT500));
      if (!AddContent(CONTENT500)) return false;
      break;
    case BAD_REQUEST:
      AddStatusLine(400, STATUS400);
      AddHeader(strlen(CONTENT400));
      if (!AddContent(CONTENT400)) return false;
      break;
    case FORBIDDEN_REQUEST: {
      AddStatusLine(403, STATUS403);
      AddHeader(strlen(CONTENT403));
      if (!AddContent(CONTENT403)) return false;
      break;
    }
    case NO_RESOURCE:
      AddStatusLine(404, STATUS404);
      AddHeader(strlen(CONTENT404));
      if (!AddContent(CONTENT404)) return false;
      break;
    case FILE_REQUEST:
      AddStatusLine(200, STATUS200);
      if (file_stat_.st_size != 0) {
        AddHeader(file_stat_.st_size);
        write_vi_[0].iov_base = write_buffer_;
        write_vi_[0].iov_len = write_index_;
        write_vi_[1].iov_base = file_address_;
        write_vi_[1].iov_len = file_stat_.st_size;
        write_vi_count_ = 2;
        bytes_send_ = write_index_ + file_stat_.st_size;
        return true;
      } else {
        AddHeader(strlen(CONTENT_EMPTY));
        if (!AddContent(CONTENT_EMPTY)) return false;
      }
    default:
      return false;
  }
  write_vi_[0].iov_base = write_buffer_;
  write_vi_[0].iov_len = write_index_;
  write_vi_count_ = 1;
  bytes_send_ = write_index_;
  return true;
}

bool HttpConn::AddResponse(const char* format, ...) {
  if (write_index_ > kWriteBufferSize) return false;
  va_list arg_list;
  va_start(arg_list, format);
  int len = vsnprintf(write_buffer_ + write_index_,
                      kWriteBufferSize - write_index_ - 1, format, arg_list);
  if (len >= kWriteBufferSize - write_index_ - 1) {
    va_end(arg_list);
    return false;
  }
  write_index_ += len;
  va_end(arg_list);
  return true;
}

bool HttpConn::AddStatusLine(int status, const char* title) {
  return AddResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConn::AddHeader(int content_len) {
  AddResponse("Content-Length:%d\r\n", content_len);
  // AddResponse("Content-Type:%s\r\n", "text/html");
  AddResponse("Connection:%s\r\n",
              (is_keep_alive_ == true) ? "keep-alive" : "close");
  return AddResponse("%s", "\r\n");
}

bool HttpConn::AddContent(const char* content) {
  return AddResponse("%s", content);
}

bool HttpConn::Write() {
  int ret;
  int content_has_send;
  if (bytes_send_ == 0) {
    ModFd(epoll_fd_, sockfd_, EPOLLIN);
    Init();
    return true;
  }
  while (true) {
    ret = writev(sockfd_, write_vi_, write_vi_count_);
    if (ret > 0) {
      bytes_have_send_ += ret;
      content_has_send = bytes_have_send_ - write_index_;
    }
    if (ret < -1) {
      if (errno == EAGAIN) {
        if (bytes_have_send_ > write_index_) {
          write_vi_[0].iov_len = 0;
          write_vi_[1].iov_base = file_address_ + content_has_send;
          write_vi_[1].iov_len = bytes_send_;
        } else {
          write_vi_[0].iov_base = write_buffer_ + bytes_have_send_;
          write_vi_[0].iov_len = write_index_ - bytes_have_send_;
        }
        ModFd(epoll_fd_, sockfd_, EPOLLOUT);
        return true;
      }
      UnMap();
      return false;
    }

    bytes_send_ -= ret;
    if (bytes_send_ <= 0) {
      UnMap();
      ModFd(epoll_fd_, sockfd_, EPOLLIN);
      if (is_keep_alive_) {
        Init();
        return true;

      } else {
        return false;
      }
    }
  }
}

void HttpConn::UnMap() {
  if (file_address_) {
    munmap(file_address_, file_stat_.st_size);
    file_address_ = 0;
  }
}