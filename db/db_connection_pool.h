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
  ~ConnectionPool() { DestroyPool(); }

 public:
  static ConnectionPool* GetInstance();
  void Init(unsigned int port, string database, string user, string password,
            unsigned int max_conn, string host);

  MYSQL* GetConnection();
  bool ReleaseConnection(MYSQL* conn);
  int GetFreeConn();
  void DestroyPool();
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