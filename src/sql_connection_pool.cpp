#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "../include/sql_connection_pool.h"

using namespace std;

//连接池构造
connection_pool::connection_pool() {
	m_CurConn = 0;
	m_FreeConn = 0;
}

//单例模式
connection_pool *connection_pool::GetInstance() {
	static connection_pool connPool;
	return &connPool;
}

//构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log) {
	m_url = url;
	m_Port = Port;
	m_User = User;
	m_PassWord = PassWord;
	m_DatabaseName = DBName;
	m_close_log = close_log;
	
	//初始化数据库连接
	for (int i = 0; i < MaxConn; ++i) {
		//MYSQL对象，用于连接服务器，MySQL标准
		MYSQL *con = nullptr;
		//分配并初始化
		con = mysql_init(con);
		if (con == nullptr) {
			LOG_ERROR("MySQL Error mysql_init");
			exit(1);
		}
		//与主机数据库建立连接
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, nullptr, 0);
		if (con == nullptr) {
			LOG_ERROR("MySQL Error mysql_real_connect");
			exit(1);
		}
		printf("succeed connect.\n");
		connList.push_back(con);
		++m_FreeConn;
	}
	m_MaxConn = m_FreeConn;

	//初始化信号量
	reserve = sem(m_FreeConn);
}

//当有请求时，从数据库连接池中取出一个连接
MYSQL *connection_pool::GetConnection() {
	MYSQL *con = nullptr;
	if (connList.size() == 0)
		return nullptr;
	
	reserve.wait();
	lock.lock();

	con = connList.front();
	connList.pop_front();
	--m_FreeConn;
	++m_CurConn;

	lock.unlock();
	return con;
}

//释放当前连接
bool connection_pool::ReleaseConnection(MYSQL *con) {
	if (con == nullptr)
		return false;
	
	lock.lock();

	connList.push_back(con);
	++m_FreeConn;
	--m_CurConn;

	lock.unlock();
	reserve.post();
	return true;
}

//销毁数据库连接池
void connection_pool::DestroyPool() {
	lock.lock();
	if (connList.size() > 0) {
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it) {
			MYSQL *con = *it;
			mysql_close(con);
		}
		m_CurConn = 0;
		m_FreeConn = 0;
		connList.clear();
	}

	lock.unlock();
}

//当前空闲的连接数
int connection_pool::GetFreeConn() {
	return m_FreeConn;
}

connection_pool::~connection_pool() {
	DestroyPool();
}


//封装类
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool) {
	//从池中获取一个连接
	*SQL = connPool->GetConnection();

	conRAII = *SQL;
	poolRAII = connPool;
}

connectionRAII::~connectionRAII() {
	poolRAII->ReleaseConnection(conRAII);
}
