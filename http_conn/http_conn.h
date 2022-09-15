#ifndef __HTTP_CONNN_H__
#define __HTTP_CONN_H___
#include "sys/socket.h"
#include "sys/epoll.h"
#include <netinet/in.h>

#define MAX_FD 65535

class HttpConn{
    private:

        sockaddr_in sockaddr_;
        int sockfd_;
    public:
        static int cur_conn_;

    public:
        HttpConn(){}
        ~HttpConn(){}

        bool init(sockaddr_in sockaddr,int sockfd);
};


#endif