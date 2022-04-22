#include "../include/httpparse.h"
#include "../include/log.h"

httpparser::httpparser() {
	//初始化
	init();
}

//初始化函數
void httpparser::init() {
	m_check_state = CHECK_STATE_REQUESTLINE;
	m_linger = false;
	m_method = GET;
	m_url = 0;
	m_version = 0;
	m_content_length = 0;
	m_host = 0;

	m_start_line = 0;
	m_checked_idx = 0;
	m_read_idx = 0;

	memset( m_read_buf, '\0', READ_BUFFER_SIZE );

	m_login_method = '0';
	m_username = 0;
	m_password = 0;
}
	
LINE_STATUS httpparser::parse_line() {
	char temp;
	for ( ; m_checked_idx < m_read_idx; ++m_checked_idx ) {
		temp = m_read_buf[ m_checked_idx ];
		if ( temp == '\r' ) {
			if ( ( m_checked_idx + 1) == m_read_idx )
				return LINE_OPEN;
			else if ( m_read_buf[ m_checked_idx + 1 ] == '\n' ) {
				//\r\n  \0\0
				m_read_buf[ m_checked_idx++ ] = '\0';
				m_read_buf[ m_checked_idx++ ] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
		else if ( temp == '\n' ) {
			if ( ( m_checked_idx > 1) && 
					m_read_buf[ m_checked_idx - 1 ] == '\r' ) {
				m_read_buf[ m_checked_idx-1 ] = '\0';
				m_read_buf[ m_checked_idx++ ] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}

HTTP_CODE httpparser::parse_request_line( char *temp ) {
	//strpbrk 在s1中找出最先含有s2的位置
	//先找出url http://
	m_url = strpbrk( temp, " \t" );
	if ( !m_url ) {
		return BAD_REQUEST;
	}
	*m_url++ = '\0';
	char *method = temp;
	//支持GET, POST
	if ( strcasecmp( method, "GET" ) == 0 ) 
		m_method = GET;
	else if (strcasecmp( method, "POST" ) == 0 )
		m_method = POST;
    else
		return BAD_REQUEST;

	//URL格式 http://IP地址:端口号/请求资源:特定参数?指定参数1&指定参数2
	
	//移动，即url字符串跳过GET
	m_url += strspn( m_url, " \t" );
	m_version = strpbrk( m_url, " \t" );
	if ( !m_version ) {
		return BAD_REQUEST;
	}
	*m_version++ = '\0';

	//使得version指向版本号，url指向http://
	m_version += strspn( m_version, " \t" );
	//仅支持HTTP/1.1
	if ( strcasecmp( m_version, "HTTP/1.1" ) != 0 ) 
		return BAD_REQUEST;
	//检查URL是否合法
	if ( strncasecmp( m_url, "http://", 7 ) == 0 ) {
		m_url += 7;
		//查找第一个/位置
		m_url = strchr( m_url, '/' );
	}
	if ( !m_url || m_url[0] != '/' )
		return BAD_REQUEST;
	//解析用户信息部分
	m_username = strchr(m_url, ':');
	if ( !m_username || m_username[0] != ':') {
        int len = strlen(m_url);
        if (len > 0 && m_url[len - 1] == '?')
            *(m_url + len - 1) = '\0';
		goto s;
    }
	//用户具体方法
	*m_username++ = '\0';
	m_login_method = m_username[0];
    if (m_method == POST)
        goto s;
	//用户名字
	m_username = strchr(m_username, '?');
	if ( !m_username || m_username[0] != '?')
		return BAD_REQUEST;
	m_username += 6;
	//用户密码
	m_password = strchr(m_username, '&');
	if ( !m_password || m_password[0] != '&')
		return BAD_REQUEST;
	*m_password++ = '\0';
	m_password += 7;

s:  LOG_INFO("The request URL is: %s\n", m_url);
	//HTTP請求行解析完成
	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

HTTP_CODE httpparser::parse_headers( char *temp ) {
	//遇到空行
	if ( temp[0] == '\0' ) {
		if ( m_content_length != 0 ) {
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	}
	else if ( strncasecmp( temp, "Connection:", 11 ) == 0 ) {
		temp += 11;
		temp += strspn( temp, " \t" );
		if ( strcasecmp( temp, "keep-alive" ) == 0 )
			m_linger = true;
	}
	else if ( strncasecmp( temp, "Content-Length:", 15 ) == 0 ) {
		temp += 15;
		temp += strspn( temp, " \t" );
		m_content_length = atol( temp );
	}
	else if ( strncasecmp( temp, "Host:", 5 ) == 0 ) {
		temp += 5;
		temp += strspn( temp, " \t" );
		m_host = temp;
	}
	else
        LOG_ERROR("I can not handle this header %s\n",temp);

	return NO_REQUEST;
}

HTTP_CODE httpparser::parse_content( char *temp ) {
	if ( m_read_idx >= ( m_content_length + m_checked_idx ) ) {
        temp[ m_content_length ] = '\0';
        if (m_method == POST && m_login_method >= '1' && m_login_method <= '3') {
            m_username = temp;
            m_username += 5;
            m_password = strchr(m_username, '&');
            *m_password++ = '\0';
            m_password += 7;
        }
        return GET_REQUEST;
	}

	return NO_REQUEST;
}

