#ifndef __DB_CONNECTION_POOL__
#define __DB_CONNECTION_POOL__

#include <mysql.h>

#include <list>
#include <string>

#include "../lock/locker.h"

using namespace std;

class ConnectionPool {
 private:
  unsigned int Port;
  string Database;
  string Host;
  string User;
  string Password;

 private:
  Locker mutex_;
  list<MYSQL*> conn_list_;
  Sem sem_;

 private:
  unsigned int MaxConn;
  unsigned int CurConn;
  unsigned int FreeConn;

 private:
  ConnectionPool() { FreeConn = CurConn = 0; };
  ~ConnectionPool() { destroyPool(); }

 public:
  static ConnectionPool* getInstance();
  void init(unsigned int port, string database, string user, string password,
            unsigned int max_conn, string host);

  MYSQL* getConnection();
  bool releaseConnection(MYSQL* conn);
  int getFreeConn();
  void destroyPool();
};

class ConnectionRAII {
 private:
  MYSQL* conn_;
  ConnectionPool* pool_;

 public:
  ConnectionRAII(MYSQL** conn, ConnectionPool* pool);
  ~ConnectionRAII();
};

#endif