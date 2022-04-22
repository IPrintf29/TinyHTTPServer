#ifndef TIMER_FUN
#define TIMER_FUN
//包含信号处理函数、定时器相关函数
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <signal.h>

#include "Time_wheel.h"
#include "epoll_fun.h"
#include "http_conn.h"

#define TIMESLOT 10

//信号处理函数
void sig_handler( int sig );

//设置信号处理函数
void addsig( int sig );

//SIGALRM信号触发
void timer_handler();

//定时器回调函数，刪除非活动连接的socket
void cb_func( client_data *user_data );

#endif
