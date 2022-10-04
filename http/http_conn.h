#ifndef __HTTP_CONNN_H__
#define __HTTP_CONN_H___
#include <netinet/in.h>

#include "../db/db_connection_pool.h"
#include "sys/epoll.h"
#include "sys/socket.h"

#define MAX_FD 65535

class HttpConn {
 private:
  static const int kFileNameLen = 200;
  static const int kReadBufferSize = 2048;
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
  MYSQL* conn;

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

 private:
  LINE_STATE ParseLine();

 public:
  HttpConn() {}
  ~HttpConn() {}

  void Init();
  void Init(sockaddr_in sockaddr, int sockfd);
  void Process();
  HTTP_CODE ProcessRead();
  void CloseConn();
  bool Read();
  bool Write();
};

#endif