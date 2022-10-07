#include "db_connection_pool.h"

#include "../log/log.h"

ConnectionPool* ConnectionPool::GetInstance() {
  static ConnectionPool conn_pool;
  return &conn_pool;
}

void ConnectionPool::Init(unsigned int port, string database, string user,
                          string password, unsigned int max_conn, string host) {
  if (max_conn <= 0) {
    LOG_ERROR("%s:%s", "db_connection_pool", "max_conn must be positive");
    exit(1);
  }
  Port = port;
  Database = database;
  User = user;
  Password = password;
  Host = host;
  mutex_.lock();
  for (int i = 0; i < max_conn; i++) {
    MYSQL* conn = NULL;
    conn = mysql_init(NULL);
    if (conn == NULL) {
      LOG_ERROR("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
      exit(1);
    }
    if (mysql_real_connect(conn, Host.c_str(), User.c_str(), Password.c_str(),
                           Database.c_str(), Port, NULL, 0) == NULL) {
      LOG_ERROR("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
      exit(1);
    }
    conn_list_.push_back(conn);
    FreeConn++;
  }
  sem_ = FreeConn;
  MaxConn = FreeConn;
  mutex_.unlock();
}

MYSQL* ConnectionPool::GetConnection() {
  MYSQL* conn = nullptr;
  if (conn_list_.empty()) {
    LOG_WARNING("%s:%s", "db_connection_pool", "db_pool busy");
    return nullptr;
  }
  sem_.wait();
  mutex_.lock();
  conn = conn_list_.front();
  conn_list_.pop_front();
  FreeConn--;
  CurConn++;
  mutex_.unlock();
  return conn;
}

bool ConnectionPool::ReleaseConnection(MYSQL* conn) {
  if (NULL == conn) return false;
  mutex_.lock();
  conn_list_.push_back(conn);
  FreeConn++;
  CurConn--;
  mutex_.unlock();
  sem_.post();
  return true;
}

int ConnectionPool::GetFreeConn() { return FreeConn; }

void ConnectionPool::DestroyPool() {
  mutex_.lock();
  if (!conn_list_.empty()) {
    list<MYSQL*>::iterator it;
    for (it = conn_list_.begin(); it != conn_list_.end(); it++) {
      MYSQL* conn = *it;
      mysql_close(conn);
    }
  }
  conn_list_.clear();
  FreeConn = 0;
  CurConn = 0;
  mutex_.unlock();
}

ConnectionRAII::ConnectionRAII(MYSQL** conn, ConnectionPool* pool) {
  pool_ = pool;
  conn_ = pool->GetConnection();
  *conn = conn_;
}

ConnectionRAII::~ConnectionRAII() { pool_->ReleaseConnection(conn_); }