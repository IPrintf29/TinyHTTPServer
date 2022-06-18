//用于封装epoll事件表相关函数

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

//错误处理函数
void error_fun(int ret, const char* s);

//设置描述符非阻塞，accept,connect,read,write等操作都立即返回
int setnonblocking( int fd );

//设置描述符添加，accept,connect,read,write等操作都立即返回
int setappend(int fd);

//设置描述符异步，accept,connect,read,write等操作都立即返回
int setasync(int fd);

//向epoll事件表中添加文件描述符及对应事件
void addfd( int epollfd, int fd, bool one_shot );

//删除epoll事件表中的文件描述符並关闭套接字
void removefd( int epollfd, int fd );

//修改epoll事件表中fd的属性
//增加ET模式，只触发一次,关闭不会触发EPOLLIN
void modfd( int epollfd, int fd, int ev );
