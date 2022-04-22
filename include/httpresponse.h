#ifndef HTTPRESPONSER
#define HTTPRESPONSER
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <cstdarg>

static const int WRITE_BUFFER_SIZE = 1024;

class httpresponse {
	public:
		//方法
		httpresponse();
		~httpresponse() {}

		//初始化函數
		void init();

		bool add_response( const char *format, ... );
		bool add_content( const char *content );
		bool add_status_line( int status, const char *title );
		bool add_content_length( int content_len );
		bool add_headers( int content_length, bool IsKeep );
		bool add_linger( bool IsKeep );
		bool add_blank_line();
	public:

		char m_write_buf[ WRITE_BUFFER_SIZE ];
		int m_write_idx;

};

#endif
