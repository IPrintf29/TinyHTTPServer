#include "../include/Timer_fun.h"
#include "../include/log.h"
#include "../include/threadpool.h"

extern bool isSync;

extern int pipefd[2];
extern client_data *timer_users;
extern time_wheel timer_lst;
extern aiocb64 *aiocbData;
extern http_conn *http_users;
extern threadpool<http_conn> *pool;

void sig_handler( int sig ){
	int save_errno = errno;
	int msg = sig;
	send( pipefd[1], (char *)&msg, 1, 0);
	errno = save_errno;
}

void sig_handler_aioread(int sig, siginfo_t *sigval, void *pad) {
	//取消异步读事件
	int fd = sigval->si_value.sival_int;
	aio_cancel64(fd, &aiocbData[fd]);
	//获取此次读取的字节数
	int read_len = (int)aio_return64(&aiocbData[fd]);
	if (read_len) {
		//修改长度
		http_users[fd].parser.m_read_idx += read_len;
		//向线程池中添加事件
		pool->append(http_users + fd);
		//修改定时器
		if (timer_users[fd].timer) {
			//移动定时器, 在此基础上加一圈 或 不变
			timer_lst.adjust_timer( timer_users[fd].timer, 0 );
			//LOG_INFO("%s", "adjust timer once\n");
		}
	}
	else {
		if ( timer_users[fd].timer )
			timer_lst.del_timer( timer_users[fd].timer );
			cb_func( &timer_users[fd] );
	}
}

void sig_handler_aiowrite(int sig, siginfo_t *sigval, void *pad) {
	//取消写完成事件
	int fd = sigval->si_value.sival_int;
	aio_cancel64(fd, &aiocbData[fd]);
	//重新初始化
	http_users[fd].init();
	//注册读完成事件
	aiocbData[fd].aio_nbytes = 1024;
	aiocbData[fd].aio_buf = http_users[fd].parser.m_read_buf;
	aiocbData[fd].aio_sigevent.sigev_signo = ASYNC_READ;

	//aio read
	aio_read64(&aiocbData[fd]);
}

void thread_handler_aioread(sigval_t sigval) {
	//取消异步读事件
	int fd = sigval.sival_int;
	aio_cancel64(fd, &aiocbData[fd]);
	//获取此次读取的字节数
	int read_len = (int)aio_return64(&aiocbData[fd]);
	if (read_len) {
		//修改长度
		http_users[fd].parser.m_read_idx += read_len;
		//向线程池中添加事件
		pool->append(http_users + fd);
		//修改定时器
		if (timer_users[fd].timer) {
			//移动定时器, 在此基础上加一圈 或 不变
			timer_lst.adjust_timer( timer_users[fd].timer, 0 );
			//LOG_INFO("%s", "adjust timer once\n");
		}
	}
	else {
		if ( timer_users[fd].timer )
			timer_lst.del_timer( timer_users[fd].timer );
			cb_func( &timer_users[fd] );
	}
}

void thread_handler_aiowrite(sigval_t sigval) {
	//取消写完成事件
	int fd = sigval.sival_int;
	aio_cancel64(fd, &aiocbData[fd]);
	//重新初始化
	http_users[fd].init();
	//注册读完成事件
	aiocbData[fd].aio_buf = http_users[fd].parser.m_read_buf;
	//线程回调方法
	aiocbData[fd].aio_sigevent.sigev_notify_function = thread_handler_aioread;

	//aio read
	aio_read64(&aiocbData[fd]);
}

void addsig( int sig ){
	struct sigaction sa;
	memset( &sa, '\0', sizeof( sa ));
	sa.sa_handler = sig_handler;
	sa.sa_flags |= SA_RESTART;
	sigfillset( &sa.sa_mask );

	int ret = sigaction( sig, &sa, NULL);
	error_fun(ret, "sigaction");
}

void addsig_aioread(int sig) {
	struct sigaction sa;
	memset( &sa, '\0', sizeof( sa ));
	sa.sa_flags |= SA_RESTART;
	sa.sa_flags |= SA_ONSTACK;
	sa.sa_flags |= SA_SIGINFO;
	sa.sa_flags |= SA_NOMASK;
	sa.sa_sigaction = sig_handler_aioread;
	sigfillset( &sa.sa_mask );

	int ret = sigaction( sig, &sa, NULL);
	error_fun(ret, "sigaction");
}

void addsig_aiowrite(int sig) {
	struct sigaction sa;
	memset( &sa, '\0', sizeof( sa ));
	sa.sa_flags |= SA_RESTART;
	sa.sa_flags |= SA_ONSTACK;
	sa.sa_flags |= SA_SIGINFO;
	sa.sa_flags |= SA_NOMASK;
	sa.sa_sigaction = sig_handler_aiowrite;
	sigfillset( &sa.sa_mask );

	int ret = sigaction( sig, &sa, NULL);
	error_fun(ret, "sigaction");
}

void timer_handler() {
	//一个SIGALRM，一次滴答
	timer_lst.tick();
	//重新发送SIGALRM信号，每3s一次
	alarm( TIMESLOT );
}

void cb_func( client_data *user_data) {
	int sockfd = user_data->sockfd;
	if (isSync)
		http_users[sockfd].close_conn_sync();
	else
		http_users[sockfd].close_conn_async();
    LOG_INFO("close fd %d\n", sockfd);
}


