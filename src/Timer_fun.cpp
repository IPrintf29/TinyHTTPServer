#include "../include/Timer_fun.h"
#include "../include/log.h"

extern int pipefd[2];
extern time_wheel timer_lst;

extern http_conn *http_users;

void sig_handler( int sig ){
	int save_errno = errno;
	int msg = sig;
	send( pipefd[1], (char *)&msg, 1, 0);
	errno = save_errno;
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

void timer_handler() {
	//一个SIGALRM，一次滴答
	timer_lst.tick();
	//重新发送SIGALRM信号，每10s一次
	alarm( TIMESLOT );
}

void cb_func( client_data *user_data ) {
	int sockfd = user_data->sockfd;
	http_users[sockfd].close_conn();
    LOG_INFO("close fd %d\n", sockfd);
}


