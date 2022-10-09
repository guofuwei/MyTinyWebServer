#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "./http/http_conn.h"
#include "./log/log.h"
#include "./signal/signal.h"
#include "./threadpool/threadpool.h"
#include "./timer/timer.h"
#include "./timer/timer_list.h"

#define MAX_EVENT_NUM 10000
#define TIMESLOT 5

// 主线程静态变量
// 管道fd
static int pipefd[2];
// epollfd
static int epollfd = -1;
// 定时器链表
static TimerList timer_list;

extern int SetNonBlocking(int fd);

extern void AddFd(int epoll_fd, int fd, bool is_shot);

extern void RemoveFd(int epoll_fd, int fd);

extern void ModFd(int epoll_fd, int fd, int ev);

void SignalHandler(int signal) {
  int save_errno = errno;
  int msg = signal;
  send(pipefd[1], (char*)&msg, 1, 0);
  errno = save_errno;
}

void TimerCallBackFunc(ClientData* client_data) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, client_data->sockfd_, 0);
  close(client_data->sockfd_);
  HttpConn::conn_count_--;
  LOG_INFO("%s:%s:%d", "main.cpp", "close", client_data->sockfd_);
}

int main() {
  // 忽略SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  // 开启监听套接字
  int port = 11000;
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in address;
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
  ret = listen(listenfd, 5);

  // 创建epoll套接字
  epoll_event events[MAX_EVENT_NUM];
  epollfd = epoll_create(5);
  if (epollfd == -1) {
    LOG_ERROR("%s:%s", "main.cpp", "create epoll fd error");
    exit(-1);
  }
  AddFd(epollfd, listenfd, false);

  // 在主函数中定义一些资源
  // 定义日志系统,单例模式
  Log* server_log = Log::GetInstance();
  server_log->Init("ServerLog", 0);
  LOG_INFO("%s", "Init success");

  // 定义数据库连接池，单例模式
  ConnectionPool* dbpool = ConnectionPool::GetInstance();
  dbpool->Init(3306, "mywebserver", "root", "");

  // 定义线程池
  ThreadPool<HttpConn>* threadpool = new ThreadPool<HttpConn>(dbpool, 4);

  // 定义http conn数组
  HttpConn* http_conns = new HttpConn[MAX_FD];
  http_conns->InitDbRet(dbpool);
  HttpConn::epoll_fd_ = epollfd;

  // 创建socketpair，注册SIGALRM,SIGTERM信号
  ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
  if (ret == -1) {
    LOG_ERROR("%s:%s", "main.cpp", "create socketpair error");
    exit(-1);
  }
  SetNonBlocking(pipefd[1]);
  AddFd(epollfd, pipefd[0], false);
  AddSignal(SIGALRM, SignalHandler, false);
  AddSignal(SIGTERM, SignalHandler, false);

  // 创建用于定时器的客户数据
  ClientData* client_data = new ClientData[MAX_FD];

  // 服务器标志位
  // 服务器主线程标识位
  bool server_stop = false;
  // 服务器处理超时链接标识位
  bool timeout = false;

  // 发送SIGALRM信号
  alarm(TIMESLOT);
  while (!server_stop) {
    int wait_number = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
    if (wait_number < 0 && errno != EINTR) {
      LOG_ERROR("%s:%s", "main.cpp", "epoll failure");
      server_stop = true;
      continue;
    }

    // 开始逐个处理socket
    for (int i = 0; i < wait_number; i++) {
      int sockfd = events[i].data.fd;

      // 有新的客户连接
      if (sockfd == listenfd) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
#ifdef LISTENFDLT
        int client_fd =
            accept(listenfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
          LOG_WARNING("%s:%s", "main.cpp", "accept error");
          continue;
        }
        if (HttpConn::conn_count_ > MAX_FD) {
          LOG_WARNING("%s:%s", "main.cpp", "server busy");
          continue;
        }
        // 初始化http conn对象
        http_conns[client_fd].Init(client_addr, client_fd);

        // 初始化ClienData
        // 初始化定时器
        client_data[client_fd].sockaddr_ = client_addr;
        client_data[client_fd].sockfd_ = client_fd;
        Timer* timer = new Timer;
        time_t now_time = time(NULL);
        timer->callback_func_ = TimerCallBackFunc;
        timer->expire_time_ = now_time + 3 * TIMESLOT;
        client_data[client_fd].timer_ = timer;
        timer->client_data_ = &client_data[client_fd];
        // 添加到timelist链表中
        timer_list.Insert(timer);
#endif
#ifdef LISTENFDET
        while (true) {
          int client_fd =
              accept(listenfd, (struct sockaddr*)&client_addr, &client_len);
          if (client_fd < 0) {
            LOG_WARNING("%s:%s", "main.cpp", "accept error");
            break;
          }
          if (HttpConn::conn_count_ > MAX_FD) {
            LOG_WARNING("%s:%s", "main.cpp", "server busy");
            break;
          }
          // 初始化http conn对象
          http_conns[client_fd].Init(client_addr, client_fd);

          // 初始化ClienData
          // 初始化定时器
          client_data[client_fd].sockaddr_ = client_addr;
          client_data[client_fd].sockfd_ = client_fd;
          Timer* timer = new Timer;
          time_t now_time = time(NULL);
          timer->callback_func_ = TimerCallBackFunc;
          timer->expire_time_ = now_time + 3 * TIMESLOT;
          client_data[client_fd].timer_ = timer;
          timer->client_data_ = &client_data[client_fd];
          // 添加到timelist链表中
          timer_list.Insert(timer);
        }
        continue;
#endif
      }

      // 处理客户端中断连接
      else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        Timer* timer = client_data[sockfd].timer_;
        timer->callback_func_(&client_data[sockfd]);
        timer_list.Delete(timer);
      }

      // 处理信号
      else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
        int signal[1024];
        int ret = recv(pipefd[0], &signal, sizeof(signal), 0);
        if (ret <= 0) {
          continue;
        } else {
          for (int i = 0; i < ret; i++) {
            switch (signal[i]) {
              case SIGTERM:
                server_stop = true;
                break;
              case SIGALRM:
                timeout = true;
                break;
            }
          }
        }
      }

      // 处理客户端的可读事件
      else if (events[i].events & EPOLLIN) {
        Timer* timer = client_data[sockfd].timer_;
        if (http_conns[sockfd].Read()) {
          threadpool->Append(&http_conns[sockfd]);
          if (timer) {
            time_t now_time = time(NULL);
            timer->expire_time_ = now_time + TIMESLOT * 3;
            timer_list.Adjust(timer);
          }
        } else {
          if (timer) {
            timer->callback_func_(&client_data[sockfd]);
            timer_list.Delete(timer);
          }
        }
      }

      // 处理客户端可写事件
      else {
        Timer* timer = client_data[sockfd].timer_;
        if (http_conns[sockfd].Write()) {
          if (timer) {
            time_t now_time = time(NULL);
            timer->expire_time_ = now_time + TIMESLOT * 3;
            timer_list.Adjust(timer);
          }
        } else {
          if (timer) {
            timer->callback_func_(&client_data[sockfd]);
            timer_list.Delete(timer);
          }
        }
      }
    }
    if (timeout) {
      timer_list.Tick();
      alarm(TIMESLOT);
      timeout = false;
    }
  }

  close(epollfd);
  close(listenfd);
  close(pipefd[0]);
  close(pipefd[1]);
  delete[] http_conns;
  delete[] client_data;
  delete threadpool;

  return 0;
}
