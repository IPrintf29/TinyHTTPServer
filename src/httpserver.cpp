#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <map>
#include <mysql/mysql.h>

#include "../include/locker.h"
#include "../include/http_conn.h"
#include "../include/httpparse.h"
#include "../include/httpresponse.h"
#include "../include/epoll_fun.h"
#include "../include/threadpool.h"
#include "../include/Timer_fun.h"
#include "../include/Time_wheel.h"
#include "../include/log.h"
#include "../include/sql_connection_pool.h"

//最大文件描述符數量
#define MAX_FD 65535
//最大事件表註冊事件
#define MAX_EVENT_NUMBER 10000

//全局變量 http_conn數組
http_conn *http_users = NULL;
//全局变量 管道用于信号
int pipefd[2];
//全局变量 时间轮用于定时关闭客户端
time_wheel timer_lst;
//全局变量 记录用户名字和密码
//map<string, string> clientsInfo;
//全局变量 程序的当前工作路径
extern char RootPath[100];

void show_error( int connfd, const char *info ) {
	send( connfd, info, strlen( info ), 0 );
	close( connfd );
}

int main( int argc, char *argv[] ) {
	if ( argc <= 2 ) {
		printf( "usage: %s ip_address port_number\n", 
		basename( argv[0] ) );
		return 1;
	}
	//参数格式 IP PORT LOGMODE
	const char *ip = argv[1];
	int port = atoi( argv[2] );
    //获取工作路径
    getcwd(RootPath, sizeof(RootPath));
    strcat(RootPath, "/HTMLsource");
	//log_mode 0 同步 1 异步
	int log_mode = 0;
	if (argc >= 3)
		log_mode = atoi(argv[3]);
	if (log_mode == 0)
		printf("Log Mode : Synchronous.\n");
	else
		printf("Log Mode : Asynchronous.\n");
	//创建线程池
	threadpool< http_conn > *pool = NULL;
	try {
		pool = new threadpool< http_conn >;
	}
	catch ( ... ) {
		return 1;
	}

	//epoll处理事件相关
	//預先为所有客戶分配一个http_conn对象
	http_users = new http_conn[ MAX_FD ];
	assert( http_users );
	int user_count = 0;

	int listenfd = socket( AF_INET, SOCK_STREAM, 0 );
	assert( listenfd >= 0 );
	//设置长短连接
	struct linger tmp = { 1, 0 };
	setsockopt( listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof( tmp ) );

	//设置地址
	int ret = 0;
	struct sockaddr_in address;
	bzero( &address, sizeof( address ) );
	address.sin_family = AF_INET;
	inet_pton( AF_INET, ip, &address.sin_addr );
	address.sin_port = htons( port );

	//绑定
	ret = bind( listenfd, ( struct sockaddr *)&address, 
	sizeof( address ) );
	assert( ret >= 0 );

	//设置监听上限
	ret = listen( listenfd, 5 );
	assert( ret >= 0 );

	//epoll事件表
	epoll_event events[ MAX_EVENT_NUMBER ];
	int epollfd = epoll_create( 5 );
	assert( epollfd != -1 );
	//添加listenfd监听事件
	addfd( epollfd, listenfd, false );
	http_conn::m_epollfd = epollfd;

	//统一事件源，处理信号相关
	//创建管道，注册读事件
	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
	error_fun( ret, "socketpair");
	//设置写管道非阻塞
	setnonblocking( pipefd[1] );
	addfd( epollfd, pipefd[0], false );

	//設置信号处理函数
	addsig( SIGHUP );      	//控制终端挂起   1    
	addsig( SIGCHLD );     	//子进程发生变化 
	addsig( SIGTERM );     	//终止进程
	addsig( SIGINT );      	//中断进程       2   Ctrl+C
	addsig( SIGALRM );		//时钟信号
	addsig( SIGPIPE );		//避免读客户端数据时意外关闭	
	
	bool stop_server = false;

	client_data *timer_users = new client_data[MAX_FD];
	bool timeout = false;  
	alarm( TIMESLOT );     //开启第一个时钟

	//日志相关
	if (log_mode == 1)
	//异步
		Log::get_instance()->init("./ServerLogFolder/ServerLog", 0, 2000, 800000, 8000);
	else
	//同步
		Log::get_instance()->init("./ServerLogFolder/ServerLog", 0, 2000, 800000, 0);
	//记录IP地址
	char *Connect_IP = new char[16]();

	//数据库相关
	connection_pool *m_connPool = connection_pool::GetInstance();
	//初始化数据库连接池，默认8个连接
	m_connPool->init("localhost", "root", "123456Abc##", "WEBSERVERUSERS", 3306, 8, 0);
	//改变线程池中数据库连接池的指向
	pool->m_connection_pool = m_connPool;
	//从users表中检索username, password数据
	MYSQL *mysql = nullptr;
	connectionRAII mysqlcon(&mysql, m_connPool);	//资源获取即初始化，从池中取出一个连接
	if (mysql_query(mysql, "SELECT username, password FROM users"))
		LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
	//从表中检索完整的结果集合
	MYSQL_RES *result = mysql_store_result(mysql);
	//返回结果集合中的列数
	int num_fields = mysql_num_fields(result);
	//返回集合中的数组
	MYSQL_FIELD *fields = mysql_fetch_fields(result);
	//依次存入LRUCache当中，并且不超出其大小
    int capacity = http_conn::clientsInfo.GetCapacity();
    //一行记录
    MYSQL_ROW row;
	while ((row = mysql_fetch_row(result)) != NULL && (http_conn::clientsInfo.GetSize() < capacity)) {
		string temp1(row[0]);
		string temp2(row[1]);
		//http_conn::clientsInfo[temp1] = temp2;
        http_conn::clientsInfo.PutValue(temp1, temp2);
	}
	//释放内存
	mysql_free_result(result);

	//处理事件循环
	while ( !stop_server ) {
		int number = epoll_wait( epollfd, events, 
		MAX_EVENT_NUMBER, -1 );
		if ( ( number < 0 ) && ( errno != EINTR ) ) {
			LOG_ERROR("epoll failure\n");
			break;
		}
		for ( int i = 0; i < number; i++ ) {
			int sockfd = events[i].data.fd;
			if ( sockfd == listenfd ) {
				struct sockaddr_in client_address;
				socklen_t client_addrlength = 
				sizeof( client_address);
                //accept连接
				int connfd = accept( listenfd, ( struct sockaddr*)
				&client_address, &client_addrlength );
				if ( connfd < 0 ) {
                    LOG_ERROR("errno is :%d\n", errno);
					continue;
				}
				if (http_conn::m_user_count >= MAX_FD ) {
					continue;
				}
				//初始化连接
				http_users[connfd].init( connfd, client_address );

				timer_users[connfd].address = client_address;
				timer_users[connfd].sockfd = connfd;
                //创建定时器 60 * 2s, 一圈
				tw_timer *timer = timer_lst.add_timer( 60 );
				timer->user_data = &timer_users[connfd];
				timer->cb_func = cb_func;
				timer_users[connfd].timer = timer;
				//日志写入，有新的连接
				inet_ntop(AF_INET, &client_address.sin_addr.s_addr, Connect_IP, INET_ADDRSTRLEN);
				LOG_INFO("New Client Connect: %s:%d", Connect_IP, client_address.sin_port);
			}
			//管道 信号事件
			else if ((sockfd == pipefd[0]) && 
			(events[i].events & EPOLLIN)){
				char signals[1024];
				ret = recv( pipefd[0], signals, 
				sizeof(signals), 0);
				if ( ret <= 0)
					continue;
				else{
					for (int i = 0; i < ret; i++)
						switch (signals[i]){
						case SIGHUP:
						case SIGCHLD:
						case SIGPIPE:
							continue;
						case SIGTERM:
						case SIGINT:{
							stop_server = true;
							break;
							}
						case SIGALRM:{
							timeout = true;
							break;
							}
						}
				}
			}
			//如果有异常，直接关闭连接
			else if ( events[i].events & ( EPOLLRDHUP | 
			EPOLLHUP | EPOLLERR ) ) {
				if ( timer_users[sockfd].timer )
					timer_lst.del_timer( timer_users[sockfd].timer );
				cb_func( &timer_users[sockfd] );
			}
			//连接套接字接收到读事件
			else if ( events[i].events & EPOLLIN ) {
				if ( http_users[ sockfd ].read() ) {
					pool->append( http_users + sockfd );
					if ( timer_users[sockfd].timer ) {
						//移动定时器, 在此基础上加一圈 或 不变
						timer_lst.adjust_timer( timer_users[sockfd].timer, 0 );
						LOG_INFO("%s", "adjust timer once\n");
					}
				}
				else {
					if ( timer_users[sockfd].timer )
						timer_lst.del_timer( timer_users[sockfd].timer );
					cb_func( &timer_users[sockfd] );
				}
			}
			//连接套接字接收到写事件
			else if ( events[i].events & EPOLLOUT ) {
				if ( !http_users[ sockfd ].write() ) {
					//LOG_ERROR("Write Error, Client Close: %s:%d", inet_ntop(timer_users[connfd].address.sin_addr.s_addr), timer_users[connfd].address.sin_port);
					if ( timer_users[sockfd].timer )
						timer_lst.del_timer( timer_users[sockfd].timer );
					cb_func( &timer_users[sockfd] );
				}
			}
			else
				return 0;
		}
		//若接收到SIGALRM，执行timer_handler()
		//timer_handler 时间轮转动一个刻度
		if ( timeout ){
			timer_handler();
			timeout = false;
		}

	}

	close( epollfd );
	close( listenfd );
	close( pipefd[1] );
	close( pipefd[0] );
	delete [] http_users;
	delete [] timer_users;
	delete pool;
	delete [] Connect_IP;
	//delete timer_lst; 
	
	return 0;
}
