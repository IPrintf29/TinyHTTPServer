#ifndef HTTPPARSER
#define HTTPPARSER
//HTTP请求读取与解析
//应用有限状态机方法

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

//HTTP请求方法
enum METHOD { GET = 0, POST, HEAD, PUT, DELETE,
	TRACE, OPTIONS, CONNECT, PATCH };
//主状态机两种可能状态：当前正在分析请求行，当前正在分析头部字段
enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, 
	CHECK_STATE_HEADER,
	CHECK_STATE_CONTENT };
//从状态机三种可能状态：读取到一个完整行，行出错，行数据不完整
enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
//服务器处理HTTP请求的结果：请求不完整，完整客户请求，
//客户请求语法错误，无访问数据源，客户没有足够权限，
//文件请求，服务器内部错误，客户端已经关闭连接。
//GET_REQUEST 解析成功的最终状态，进入do_request()
//FILE_REQUEST 做出响应成功的最终状态。
enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_REQUEST };

//读缓冲区大小
static const int READ_BUFFER_SIZE = 1024;

//HTTP请求读取解析类
class httpparser {

	public:
	//方法
	httpparser();
	~httpparser() {};

	//初始化函数
	void init();
	//从buffer中解析出一行
	LINE_STATUS parse_line();

	//解析请求行
	HTTP_CODE parse_request_line( char *temp );
	//解析头部信息
	HTTP_CODE parse_headers( char *temp );
	//解析HTTP请求消息体
	HTTP_CODE parse_content( char *temp );

	char *get_line() { return m_read_buf + 
	m_start_line; }

	public:
	//变量
	//读缓冲区
	char m_read_buf[ READ_BUFFER_SIZE ];
	//读缓冲中已经读入数据的下一位置
	int m_read_idx;
	//当前正在分析的字符在读缓冲区中的位置
	int m_checked_idx;
	//当前正在解析的行的起始位置
	int m_start_line;
	
	//主状态机当前状态
	CHECK_STATE m_check_state;
	//请求方法
	METHOD m_method;
	
	//客户请求文件名字
	char *m_url;
	//HTTP版本协议号，仅支持HTTP/1.1
	char *m_version;
	//主机名字
	char *m_host;
	//HTTP请求的消息体长度
	int m_content_length;
	//HTTP请求是否保持连接
	int m_linger;

	//用于数据库记录用户名字和密码
	char m_login_method;	//1. 注册 2. 登录 3. 修改密码
	char *m_username;
	char *m_password;
};

#endif

