#ifndef __EPOLL_H__
#define __EPOLL_H__

#include <fcntl.h>
#include <unistd.h>

#include "sys/epoll.h"
#include "sys/socket.h"

//#define CONNFDET //边缘触发非阻塞
#define CONNFDLT  //水平触发阻塞

//#define LISTENFDET //边缘触发非阻塞
#define LISTENFDLT  //水平触发阻塞

//对文件描述符设置非阻塞
int SetNonBlocking(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

void AddFd(int epoll_fd, int fd, bool is_shot) {
  epoll_event event;
  event.data.fd = fd;
#ifdef CONNFDET
  event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
#endif

#ifdef CONNFDLT
  event.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef LISTENFDET
  event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
#endif

#ifdef LISTENFDLT
  event.events = EPOLLIN | EPOLLRDHUP;
#endif

  if (is_shot) event.events |= EPOLLONESHOT;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
  SetNonBlocking(fd);
}

void RemoveFd(int epoll_fd, int fd) {
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
  close(fd);
}

void ModFd(int epoll_fd, int fd, int ev) {
  epoll_event event;
  event.data.fd = fd;
#ifdef CONNFDET
  event.events = ev | EPOLLONESHOT | EPOLLRDHUP | EPOLLET;
#endif

#ifdef CONNFDLT
  event.events = ev | EPOLLRDHUP | EPOLLONESHOT;
#endif

  epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
}

#endif