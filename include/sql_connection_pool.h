#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "locker.h"
#include "log.h"

using namespace std;

class connection_pool {
public:
	MYSQL *GetConnection();			//获取数据库连接
	bool ReleaseConnection(MYSQL *conn);	//释放数据库连接
	int GetFreeConn();			//获取线程池中线程连接
	void DestroyPool();			//销毁线程池中线程

	//单例模式
	static connection_pool *GetInstance();
	//初始化
	void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);

private:
	connection_pool();
	~connection_pool();

	int m_MaxConn;		//最大连接数
	int m_CurConn;		//当前连接数
	int m_FreeConn;		//空闲连接数
	list<MYSQL *> connList;	//连接池
	locker lock;		//互斥锁
	sem reserve;		//信号量

public:
	string m_url;		//主机地址
	string m_Port;		//数据库端口号
	string m_User;		//登录数据库用户名
	string m_PassWord;	//登录数据库密码
	string m_DatabaseName;	//使用的数据库名字
	int m_close_log;	//是否开启日志
};

//封装类，资源获取即初始化
class connectionRAII {
public:
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();

private:
	MYSQL *conRAII;
	connection_pool *poolRAII;
};

#endif
