#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "../http/http_conn.h"

#define MAX_EVENT_NUM 10000

int main() {
  int port = 5000;
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in address;
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
  ret = listen(listenfd, 5);
  epoll_event events[MAX_EVENT_NUM];
  int epollfd = epoll_create(5);

  // 在主函数中定义一些资源
  HttpConn* http_conns = new HttpConn[MAX_FD];

  while (true) {
    int wait_number = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);

    for (int i = 0; i < wait_number; i++) {
      int sockfd = events[i].data.fd;

      // New client connect
      if (sockfd == listenfd) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd =
            accept(listenfd, (struct sockaddr*)&client_addr, &client_len);
        http_conns[client_fd].Init(client_addr, client_fd);
      }
    }
  }

  return 0;
}
