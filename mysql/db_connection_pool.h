#ifndef __DB_CONNECTION_POOL__
#define __DB_CONNECTION_POOL__

#include <mysql.h>

#include <list>
#include <string>

#include "../lock/locker.h"

using namespace std;

class connection_pool {
 private:
  string Url;
  string Port;
  string Database;
  string Host;
  string User;
  string Password;

 private:
  Locker mutex_;
  list<MYSQL*> connList;
  Sem sem_;

 private:
  unsigned int MaxConn;
  unsigned int CurConn;
  unsigned int FreeConn;

 private:
  connection_pool();
  ~connection_pool();

 public:
  static connection_pool* getInstance();
  void init(string url, string port, string database, string user,
            string password, string host);

  MYSQL* getConnection();
  bool ReleaseConnection(MYSQL* conn);
  int getFreeConn();
  void destroyPool();
};

#endif