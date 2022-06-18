#ifndef TIMER_FUN
#define TIMER_FUN
//包含信号处理函数、定时器相关函数
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <signal.h>
#include <aio.h>

#include "Time_wheel.h"
#include "epoll_fun.h"
#include "http_conn.h"

#define TIMESLOT 3
#define ASYNC_READ SIGUSR1
#define ASYNC_WRITE SIGUSR2

//信号处理函数
void sig_handler( int sig );
void sig_handler_aioread(int sig, siginfo_t *sigval, void *pad);
void sig_handler_aiowrite(int sig, siginfo_t *sigval, void *pad);

//线程回调方法
void thread_handler_aioread(sigval_t sigval);
void thread_handler_aiowrite(sigval_t sigval);

//设置信号处理函数
void addsig( int sig );
void addsig_aioread(int sig);
void addsig_aiowrite(int sig);

//SIGALRM信号触发
void timer_handler();

//定时器回调函数，刪除非活动连接的socket
void cb_func( client_data *user_data);

#endif
