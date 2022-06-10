#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "../include/log.h"

using namespace std;

Log::Log() {
	//初始化行数
	m_count = 0;
	//初始为同步IO
	m_is_async = false;
}

Log::~Log() {
	if (m_fp != nullptr)
		fclose(m_fp);
}

//Log日志系统初始化
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size) {
	//如果设置了队列长度，异步
	if (max_queue_size >= 1) {
		m_is_async = true;
		m_log_queue = new log_queue<string>(max_queue_size);
		pthread_t tid;
		pthread_create(&tid, nullptr, flush_log_thread, nullptr);
		pthread_detach(tid);
	}
	else
		m_is_async = false;
	//初始化参数值
	m_close_log = close_log;
	m_log_buf_size = log_buf_size;
	m_buf = new char[m_log_buf_size];
	memset(m_buf, '\0', m_log_buf_size);
	m_split_lines = split_lines;

	//当前时间
	time_t t = time(nullptr);
	struct tm *sys_tm = localtime(&t);
	struct tm my_tm = *sys_tm;

	//文件名字
	const char *p = strrchr(file_name, '/');
	char log_full_name[256] = {0};
	if (p == nullptr)
	//默认命名规则
	//位于当前文件夹
	//2021_12_28_filename
		snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
	else {
	//将file_name拆分成dir_name文件夹名和log_name日志文件名
		strcpy(log_name, p + 1);
		strncpy(dir_name, file_name, p - file_name + 1);
	//dir_name2021_12_28_filename
		snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
	}
	//当天
	m_today = my_tm.tm_mday;
	//创建并打开日志文件
	m_fp = fopen(log_full_name, "a");
	if (m_fp == nullptr)
		return false;
	return true;
}

void Log::write_log(int level, const char *format, ...) {
	//当前时间
	struct timeval now = {0, 0};
	gettimeofday(&now, nullptr);
	time_t t = now.tv_sec;
	struct tm *sys_tm = localtime(&t);
	struct tm my_tm = *sys_tm;
	//1. 日志等级
	char s[16] = {0};
	switch (level) {
	case 0:
		strcpy(s, "[debug]:");
		break;
	case 1:
		strcpy(s, "[info]:");
		break;
	case 2:
		strcpy(s, "[warn]:");
		break;
	case 3:
		strcpy(s, "[erro]:");
		break;
	default:
		strcpy(s, "[info]:");
		break;
	}
	//2. 写入内容
	m_mutex.lock();
	m_count++;
	//第二天或超过行数要创建新日志文件
	if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) {
		//刷新缓冲区，关闭旧文件
		fflush(m_fp);
		fclose(m_fp);
		//创建新文件
		printf("Create New Log\n");
		char new_log[256] = {0};
		char tail[16] = {0};
		snprintf(tail, 16, "%d_%02d_%02d", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
		//1. 第二天
		if (m_today != my_tm.tm_mday) {
			snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
			m_today = my_tm.tm_mday;
			m_count = 0;
		}
		else
		//2. 超过行数
			snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
		//打开新文件
		m_fp = fopen(new_log, "a");
	}
	m_mutex.unlock();

	//写入数据
	va_list valst;
	va_start(valst, format);
	string log_str;

	m_mutex.lock();
	int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s", 
	my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, 
	my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
	int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
	m_buf[n + m] = '\n';
	m_buf[n + m + 1] = '\0';
	log_str = m_buf;
	m_mutex.unlock();

	//异步，加入队列中
	if (m_is_async && !m_log_queue->full()) {
		m_log_queue->push(log_str);
	}
	//同步，直接打印
	else {
		m_mutex.lock();
		fputs(log_str.c_str(), m_fp);
		m_mutex.unlock();
	}

	va_end(valst);
}

//刷新缓冲区
void Log::flush(void) {
	m_mutex.lock();
	fflush(m_fp);
	m_mutex.unlock();
}
