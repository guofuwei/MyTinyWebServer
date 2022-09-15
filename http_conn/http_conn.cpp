#include "http_conn.h"


bool HttpConn::init(sockaddr_in sockaddr,int sockfd)
{
    if(cur_conn_==MAX_FD-1)
    {
        return false;
    }
    cur_conn_++;
    sockaddr_=sockaddr;
    sockfd_=sockfd;
    return true;
}