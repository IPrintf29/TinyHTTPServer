//封装http 读取，解析，应答

#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <mysql/mysql.h>
#include <map>
#include <string>

#include "locker.h"
#include "httpparse.h"
#include "httpresponse.h"
#include "epoll_fun.h"
#include "LRUCache.h"

using namespace std;

class http_conn {
	public:
		//文件名最大长度
		static const int FILENAME_LEN = 200;
	public:
		http_conn() {}
		~http_conn() {}

	public:
		//初始化新接收的连接		
		void init( int sockfd, const sockaddr_in &addr );
		//关闭连接
		void close_conn( bool real_close = true );
		//处理客户请求
		void process();
		//非阻塞读操作
		bool read();
		//非阻塞写操作
		bool write();

	private:
		//初始化连接
		void init();
		//解析HTTP请求
		HTTP_CODE process_read();
		//填充HTTP应答
		bool process_write( HTTP_CODE ret );

		HTTP_CODE do_request();
		void unmap();


	public:
		//epoll內核事件表，所有socket事件共用一份
		static int m_epollfd;
		static int m_user_count;
		//记录用户名字和密码
		//static map<string, string> clientsInfo;
        static LRUCache<string> clientsInfo;
		
		//当执行此任务时，可以获得数据库连接池中的一个连接
		MYSQL *m_mysql;
	
	private:
		int m_sockfd;
		sockaddr_in m_address;
		
		httpparser parser;
		httpresponse responser;

		//客戶请求文件的完整路径，內容 = doc_root + m_url
		char m_read_file[ FILENAME_LEN ];
		//客戶请求的目标文件被mmap到內存中的起始位置
		char *m_file_address;
		struct stat m_file_stat;
		//采用writev执行写操作
		//iovec定义两个内存块
		//m_iv[0]存储m_write_buf內容，即应答状态、头部信息
		//m_iv[1]存储请求文件内容
		struct iovec m_iv[2];
		int m_iv_count;

		//数据库操作锁
		locker sql_mtx;
};

#endif
