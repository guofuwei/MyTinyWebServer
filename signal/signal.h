#ifndef __SIGNAL_H__
#define __SIGNAL_H__
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>

bool AddSignal(int signal, void(handler)(int), bool restart = true) {
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));

  sa.sa_handler = handler;
  if (restart) {
    sa.sa_flags |= SA_RESTART;
  }
  sigfillset(&sa.sa_mask);

  return sigaction(signal, &sa, NULL) != -1;
}

#endif