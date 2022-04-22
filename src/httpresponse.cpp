#include "../include/httpresponse.h"

httpresponse::httpresponse() {
	//初始化函數
	init();
}

void httpresponse::init() {

	m_write_idx = 0;
	memset( m_write_buf, '\0', WRITE_BUFFER_SIZE );
}

bool httpresponse::add_response( const char *format, ... ) {
	if ( m_write_idx >= WRITE_BUFFER_SIZE )
		return false;
	
	va_list arg_list;
	va_start( arg_list, format );
	int len = vsnprintf( m_write_buf + m_write_idx, 
	WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list );
	
	if ( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx ) )
		return false;
	
	m_write_idx += len;
	va_end( arg_list );
	return true;
}

bool httpresponse::add_status_line( int status, const char *title ) {
	return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

bool httpresponse::add_headers( int content_len, bool IsKeep ) {
	add_content_length( content_len );
	add_linger( IsKeep );
	//空白行
	add_blank_line();
}

//写入长度
bool httpresponse::add_content_length( int content_len ) {
	return add_response( "Content-Length: %d\r\n", content_len );
}

//写入是否长连接
bool httpresponse::add_linger( bool IsKeep ) {
	return add_response( "Connection: %s\r\n", 
	( IsKeep ) ? "Keep-alive" : "close" );
}

//写入空白行
bool httpresponse::add_blank_line() {
	return add_response( "%s", "\r\n" );
}

//写入信息体
bool httpresponse::add_content( const char *content ) {
	return add_response( "%s", content );
}
