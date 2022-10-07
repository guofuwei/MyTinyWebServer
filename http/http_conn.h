#ifndef __HTTP_CONNN_H__
#define __HTTP_CONN_H___
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "../db/db_connection_pool.h"
#include "sys/epoll.h"
#include "sys/socket.h"

#define MAX_FD 65535

class HttpConn {
 private:
  static const int kFileNameLen = 200;
  static const int kReadBufferSize = 4096;
  static const int kWriteBufferSize = 1024;
  enum METHOD { GET, POST };
  enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONTENT
  };
  enum HTTP_CODE {
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
  };
  enum LINE_STATE { LINE_OK, LINE_BAD, LINE_OPEN };

 public:
  static int epoll_fd_;
  static int conn_count_;
  MYSQL* conn_;

 private:
  sockaddr_in sockaddr_;
  int sockfd_;

  char read_buffer_[kReadBufferSize];
  int read_index_;
  char write_buffer_[kWriteBufferSize];
  int write_index_;
  int checked_index_;
  int line_start_index_;

  CHECK_STATE check_state_;
  METHOD request_method_;

  char* http_method_;
  char* http_url_;
  char* http_version_;
  bool is_post_;
  bool is_keep_alive_;
  char* host_;
  unsigned int content_length_;
  char* request_body_;

  char real_file_[kFileNameLen];
  struct stat file_stat_;
  char* file_address_;

  struct iovec write_vi_[2];
  int write_vi_count_;

  int bytes_send_;
  int bytes_have_send_;

 private:
  LINE_STATE ParseLine();
  HTTP_CODE ParseRequestLine(char* text);
  HTTP_CODE ParseHeader(char* text);
  HTTP_CODE ParseContent(char* text);
  HTTP_CODE DoRequest();
  HTTP_CODE DoResponse();
  bool AddResponse(const char* format, ...);
  bool AddStatusLine(int status, const char* title);
  bool AddHeader(int content_len);
  bool AddContent(const char* content);

  // 取消文件的内存映射
  void UnMap();

 public:
  HttpConn() {}
  ~HttpConn() {}

  void InitDbRet(ConnectionPool* pool);
  void Init();
  void Init(sockaddr_in sockaddr, int sockfd);
  void Process();
  HTTP_CODE ProcessRead();
  bool ProcessWrite(HTTP_CODE read_code);
  void CloseConn();
  bool Read();
  bool Write();
};

#endif