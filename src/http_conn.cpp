#include "../include/http_conn.h"
#include "../include/log.h"
#include "../include/Timer_fun.h"
#include "limits.h"

const char *ok_200_title = "OK";
const char *error_400_title = "BAD Request";
const char *error_400_form = "Your request has bad syntax or is inherently \ 
	impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file from \
	this server.\n";
const char *error_404_title = "Not found";
const char *error_404_form = "The requested file was not found on this \
	this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the \
	requested file\n";

extern bool isSync;
extern aiocb64 *aiocbData;
extern http_conn *http_users;

//网站根目录，不能使用相对路径
char RootPath[100];

//epoll事件表
int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;
//map<string, string> http_conn::clientsInfo;
LRUCache<string> http_conn::clientsInfo(50);

void http_conn::init_sync( int sockfd, const sockaddr_in &addr ) {
	m_sockfd = sockfd;
	m_address = addr;
	//避免TIME_WAIT，端口复用
	int reuse = 1;
	setsockopt( m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
	addfd( m_epollfd, sockfd, true );
	m_user_count++;
	init();
}

void http_conn::init_async(int sockfd, const sockaddr_in &addr) {
	m_sockfd = sockfd;
	m_address = addr;
	//避免TIME_WAIT，端口复用
	int reuse = 1;
	setsockopt( m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
	//setnonblocking(sockfd);
	setasync(sockfd);
	setappend(sockfd);
	init();
}

void http_conn::init() {
	parser.init();
	responser.init();
	memset( m_read_file, '\0', FILENAME_LEN );
	m_mysql = nullptr;
}

void http_conn::close_conn_sync( bool real_close ) {
	if ( real_close && ( m_sockfd != -1 ) ) {
		removefd( m_epollfd, m_sockfd );
		m_sockfd = -1;
		m_user_count--;
	}
}

void http_conn::close_conn_async( bool real_close ) {
	if ( real_close && ( m_sockfd != -1 ) )
		m_sockfd = -1;
}


//读操作：
//read()将数据读入m_read_buf缓冲区
//process_read()解析操作的入口

//循环读取客户数据，直到无数据或者对方关闭连接
bool http_conn::read() {
	if ( parser.m_read_idx >= READ_BUFFER_SIZE )
		return false;
	
	int bytes_read = 0;
	while ( true ) {
		bytes_read = recv( m_sockfd, parser.m_read_buf + 
		parser.m_read_idx, READ_BUFFER_SIZE - parser.
		m_read_idx, 0 );
		if ( bytes_read == -1 ) {
			//如果错误是没有可读数据，则退出循环
			if (errno == EAGAIN || errno == EWOULDBLOCK )
				break;
			//其它错误
			return false;
		}
		//客户端主动关闭连接
		else if ( bytes_read == 0 )
			return false;
		parser.m_read_idx += bytes_read;
	}
	return true;
}

//分析HTTP请求的入口函数
HTTP_CODE http_conn::process_read() {
	//记录当前行读取状态
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char *text = 0;

	//主状态机读取完整行
	//循环两种情况：
	//1. 读取到消息体，单独处理
	//2. 读取到HTTP请求行，头部信息，采用解析函数
	while ( ( ( parser.m_check_state == CHECK_STATE_CONTENT ) && 
	( line_status == LINE_OK ) ) || ( ( line_status = parser.parse_line() ) 
	== LINE_OK ) ) {
		text = parser.get_line();
		parser.m_start_line = parser.m_checked_idx;
		//printf( "got 1 http line: %s\n", text );
		//主状态机状态
		switch ( parser.m_check_state ) {
			case CHECK_STATE_REQUESTLINE: {
			ret = parser.parse_request_line( text );
			if ( ret == BAD_REQUEST )
				return BAD_REQUEST;
			break;
			}
			case CHECK_STATE_HEADER: {
			ret = parser.parse_headers( text );
			if ( ret == BAD_REQUEST )
				return BAD_REQUEST;
			else if ( ret == GET_REQUEST )
				//无消息体，直接do_request()
				return do_request();
			break;
			}
			case CHECK_STATE_CONTENT: {
				ret = parser.parse_content( text );
				if ( ret == GET_REQUEST )
				//分析消息体
					return do_request();
				//没有read完整
				line_status = LINE_OPEN;
				break;
			}
			default:
				return INTERNAL_ERROR;
		}
	}
	//没有读取到完整行
	if ( line_status == LINE_OPEN )
		return NO_REQUEST;
	else
		return BAD_REQUEST;
}

//写函数
//process_write 將需要写的内容填入m_iv[2]
//m_iv[0]为m_write_buf内容，m_iv[1]为m_file_address内容，请求文件

bool http_conn::write() {
	int temp = 0;
	int bytes_have_send = 0;
	int bytes_to_send = responser.m_write_idx;
	if ( bytes_to_send == 0 ) {
		//重新注册读事件
		modfd( m_epollfd, m_sockfd, EPOLLIN );
		init();
		return false;
	}

	while ( 1 ) {
		temp = writev( m_sockfd, m_iv, m_iv_count );
		if ( temp <= -1 ) {
			if ( errno == EAGAIN ) {
				modfd( m_epollfd, m_sockfd, EPOLLIN );
				return true;
			}
			unmap();
			return false;
		}

		bytes_to_send -= temp;
		bytes_have_send += temp;
		if ( bytes_to_send <= bytes_have_send ) {
			unmap();
			if ( parser.m_linger ) {
				init();
				modfd( m_epollfd, m_sockfd, EPOLLIN );
				return true;
			}
			else {
				modfd( m_epollfd, m_sockfd, EPOLLIN );
				return false;
			}
		}
	}
}

bool http_conn::process_write( HTTP_CODE ret ) {
	switch ( ret ) {
	case INTERNAL_ERROR: {
		responser.add_status_line( 500, error_500_title );
		responser.add_headers( strlen(error_500_form ), parser.m_linger );
		if ( ! responser.add_content( error_500_form ) )
			return false;
		break;
		}
	case BAD_REQUEST: {
		responser.add_status_line( 400, error_400_title );
		responser.add_headers( strlen(error_400_form ), parser.m_linger );
		if ( ! responser.add_content( error_400_form ) )
			return false;
		break;
		}
	case NO_RESOURCE: {
		responser.add_status_line( 404, error_404_title );
		responser.add_headers( strlen(error_404_form ), parser.m_linger );
		if ( ! responser.add_content( error_404_form ) )
			return false;
		break;
		}
	case FORBIDDEN_REQUEST: {
		responser.add_status_line( 403, error_403_title );
		responser.add_headers( strlen(error_403_form ), parser.m_linger );
		if ( ! responser.add_content( error_403_form ) )
			return false;
		break;
		}
	case FILE_REQUEST: {
		responser.add_status_line( 200, ok_200_title );
		//请求文件存在
		if ( m_file_stat.st_size != 0 ) {
			responser.add_headers( m_file_stat.st_size, 
			parser.m_linger );
			//定义内存块的首地址和长度
			m_iv[0].iov_base = responser.m_write_buf;
			m_iv[0].iov_len = responser.m_write_idx;
			m_iv[1].iov_base = m_file_address;
			m_iv[1].iov_len = m_file_stat.st_size;
			m_iv_count = 2;
			return true;
		}
		else {
			const char *ok_string = "<html><body></body></html>";
			responser.add_headers( strlen( ok_string ), 
			parser.m_linger );
			if ( ! responser.add_content( ok_string ) )
				return false;
		}
		break;
	}
	default:
		return false;
	}
	//如果add_content正确，统一执行以下语句
	m_iv[0].iov_base = responser.m_write_buf;
	m_iv[0].iov_len = responser.m_write_idx;
	m_iv_count = 1;
	return true;
}

//根据解析判断出请求文件，进入do_request()
HTTP_CODE http_conn::do_request() {
	//复制根目录
	strcpy( m_read_file, RootPath );
	int len = strlen( RootPath );
	//注册，登录新增，需要根据具体参数返回不同的html
	char enroll_success_html[20] = "/EnrollSuccess.html";
	char enroll_fail_html[20] = "/EnrollFail.html";
	char login_success_html[20] = "/LoginSuccess.html";
	char login_fail_html[20] = "/LoginFail.html";
	char modify_success_html[20] = "/ModifySuccess.html";
	char modify_fail_html[20] = "/ModifyFail.html";

	//char *sql_statement = nullptr;
    //SQL 语句查询结果
    MYSQL_RES *result = nullptr;
    //cache / sql 中的password，查询得
    string cache_password;
    //所有可能用到的SQL语句
	int res = 0;
    char sql_select[200], sql_insert[200], sql_update[200];
    //select 登录，注册，修改密码都要使用
    sprintf(sql_select, "SELECT password FROM users WHERE username = '%s'", parser.m_username);
    //insert 注册使用
    sprintf(sql_insert, "INSERT INTO users(username, password) VALUES('%s', '%s')", parser.m_username, parser.m_password);
    //update 修改密码使用
    sprintf(sql_update, "UPDATE users SET password = '%s' WHERE username = '%s'", parser.m_password, parser.m_username);

	switch (parser.m_login_method) {
	case '1':	//注册
		//先查询缓存
        if (!clientsInfo.GetValue(parser.m_username, cache_password)) {
            //如果未命中，查询数据库
            res = mysql_query(m_mysql, sql_select);
            if (res) {
		        parser.m_url = enroll_fail_html;
                break;
            }
            
            result = mysql_store_result(m_mysql);
            if (mysql_num_rows(result) == 0) {
                //确实没有，只添加至数据库，不加入缓存
                res = mysql_query(m_mysql, sql_insert);

			    if (!res)
				    parser.m_url = enroll_success_html; 
			    else
				    parser.m_url = enroll_fail_html;
            } else
		        parser.m_url = enroll_fail_html;
        }
        else
			parser.m_url = enroll_fail_html;
		break;
	case '2':	//登录
        //先查询缓存
        if (clientsInfo.GetValue(parser.m_username, cache_password)) {
            if (cache_password == parser.m_password)
                parser.m_url = login_success_html;
            else
                parser.m_url = login_fail_html;
        }
        //再查询数据库
        else {
            res = mysql_query(m_mysql, sql_select);
            if (res) {
		        parser.m_url = login_fail_html;
                break;
            }
            
            result = mysql_store_result(m_mysql);
            //数据库也未命中
            if (mysql_num_rows(result) == 0) {
		        parser.m_url = login_fail_html;
                break;
            }

            MYSQL_ROW row = mysql_fetch_row(result);
            cache_password = row[0];
            if (cache_password == parser.m_password) {
                parser.m_url = login_success_html;
                //加入到缓存当中
                clientsInfo.PutValue(parser.m_username, cache_password);
            }
            else
                parser.m_url = login_fail_html;
        }
		break;
    case '3':   //修改密码
        //先查询数据库，是否存在对应用户
        res = mysql_query(m_mysql, sql_select);
        if (res) {
		    parser.m_url = modify_fail_html;
            break;
        }
            
        result = mysql_store_result(m_mysql);
        //不存在对应用户
        if (mysql_num_rows(result) == 0) {
		    parser.m_url = modify_fail_html;
            break;
        }
        
        //先更新数据库，再删除缓存方式
        res = mysql_query(m_mysql, sql_update);
        if (res) {
		    parser.m_url = modify_fail_html;
            break;
        }
        clientsInfo.DelValue(parser.m_username);
        parser.m_url = modify_success_html;
        break;
	default:
		break;
	}
	//写入日志
	if (parser.m_url == enroll_success_html)
		LOG_INFO("New User: %s, Enroll Succeed.", parser.m_username);
	if (parser.m_url == enroll_fail_html)
		LOG_INFO("New User: %s, Enroll Failed.", parser.m_username);
	if (parser.m_url == login_success_html)
		LOG_INFO("Client User: %s, Login Succeed.", parser.m_username);
	if (parser.m_url == login_fail_html)
		LOG_INFO("Client User: %s, Login Failed.", parser.m_username);
	if (parser.m_url == modify_success_html)
		LOG_INFO("Client User: %s, Modify password Success.", parser.m_username);
	if (parser.m_url == modify_fail_html)
		LOG_INFO("Client User: %s, Modify password Failed.", parser.m_username);
	//字符串拼接，请求文件的绝对路径
	strncpy( m_read_file + len, parser.m_url, FILENAME_LEN - len - 1 );
	//判断是否存在文件，若存在，返回属性
	if ( stat( m_read_file, &m_file_stat ) < 0 )
		return NO_RESOURCE;
	//判断权限 S_IROTH 其他读，is read other?
	if (! ( m_file_stat.st_mode & S_IROTH ) )
		return FORBIDDEN_REQUEST;
	//判断是否是目录
	if ( S_ISDIR( m_file_stat.st_mode ) )
		return BAD_REQUEST;

	//以只读方式打开文件
	int fd = open( m_read_file, O_RDONLY );
	//內存映射,进程间共享內存
	//MAP_PRIVATE 內存块进程私有，不修改源文件
	m_file_address = ( char *)mmap( 0, m_file_stat.st_size, PROT_READ, 
	MAP_PRIVATE, fd, 0 );

	close( fd );
	return FILE_REQUEST;
}

//对内存块munmap
void http_conn::unmap() {
	if ( m_file_address ) {
		munmap( m_file_address, m_file_stat.st_size );
		m_file_address = 0;
	}
}

//线程池中的工作线程调用，HTTP请求的入口函数
void http_conn::process() {
	//解析读的数据
	HTTP_CODE read_ret = process_read();
	//若无数据，继续注册读事件
	if ( read_ret == NO_REQUEST ) {
		if (isSync) {
			modfd( m_epollfd, m_sockfd, EPOLLIN );
			return;
		}
		else {
			//cout << "no request" << endl;
			//注册读完成事件
			aiocbData[m_sockfd].aio_buf = http_users[m_sockfd].parser.m_read_buf + 
				http_users[m_sockfd].parser.m_read_idx;
			//信号通知方法
			aiocbData[m_sockfd].aio_sigevent.sigev_signo = ASYNC_READ;
			aio_read64(&aiocbData[m_sockfd]);
			return;
		}
	}

	//解析，写应答数据
	bool write_ret = process_write( read_ret );
	if (isSync) {
		//如果解析写出错，直接关闭连接
		if ( !write_ret )
			close_conn_sync();
		//有数据可写，注册写事件
		modfd( m_epollfd, m_sockfd, EPOLLOUT );
	}
	else {
		if (!write_ret)
			close_conn_async();
		//注册写完成事件
		aiocbData[m_sockfd].aio_nbytes = http_users[m_sockfd].responser.m_write_idx;
		aiocbData[m_sockfd].aio_buf = http_users[m_sockfd].responser.m_write_buf;
		//信号通知方法
		aiocbData[m_sockfd].aio_sigevent.sigev_signo = ASYNC_WRITE;
		//线程调度方法
		//aiocbData[m_sockfd].aio_sigevent.sigev_notify_function = thread_handler_aiowrite;
		aio_write64(&aiocbData[m_sockfd]);
	}
}
