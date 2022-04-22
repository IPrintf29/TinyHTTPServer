#include "../include/epoll_fun.h"

//错误处理函数
void error_fun(int ret, const char* s){
	if (ret < 0){
		perror(s);
		exit(1);
	}
}

int setnonblocking( int fd ) {
	int old_option = fcntl( fd, F_GETFL );
	int new_option = old_option | O_NONBLOCK;
	fcntl( fd, F_SETFL, new_option );
	return old_option;
}

void addfd( int epollfd, int fd, bool one_shot ) {
	struct epoll_event event;
	event.data.fd = fd;
	//EPOLLRDHUP 当客户端关闭连接时，不会再次触发EPOLLIN
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	//EPOLLONESHOT epoll事件只触发一次
	if ( one_shot )
		event.events |= EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

void removefd( int epollfd, int fd ) {
	epoll_ctl( epollfd, EPOLL_CTL_DEL, fd, 0 );
	close( fd );
}

//修改epoll事件表中fd的属性
//增加ET模式，只触发一次，关闭不会触发EPOLLIN
void modfd( int epollfd, int fd, int ev ) {
	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl( epollfd, EPOLL_CTL_MOD, fd, &event );
}


