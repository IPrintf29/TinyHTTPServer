#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "log_queue.h"

using namespace std;

class Log {
public:
	//C++11开始，局部静态变量懒汉模式不加锁
	//局部静态变量
	//创建，刚开始运行时
	//初始化，第一次执行初始化语句
	//之后再次执行时，不会再初始化
	static Log *get_instance() {
		static Log instance;
		return &instance;
	}
	//异步写线程执行函数
	//必须静态，原因和线程池一样
	static void *flush_log_thread(void *args) {
		Log::get_instance()->async_write_log();
	}

	//初始化
	bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
	//写日志接口
	void write_log(int level, const char *format, ...);
	//刷新缓冲区
	void flush(void);

private:
	Log();
	virtual ~Log();
	//线程内部执行函数，异步写
	void *async_write_log() {
		string single_log;
		while (m_log_queue->pop(single_log)) {
			m_mutex.lock();
			fputs(single_log.c_str(), m_fp);
			m_mutex.unlock();
		}
	}
private:
	char dir_name[128];	//路径名字
	char log_name[128];	//log文件名字
	int m_split_lines;	//最大行数
	int m_log_buf_size;	//缓冲区大小
	long long m_count;	//日志行数记录
	int m_today;

	FILE *m_fp;		//文件指针
	char *m_buf;		//文件内容缓存
	log_queue<string> *m_log_queue;
	bool m_is_async;	//是否异步
	locker m_mutex;		//队列互斥锁
	int m_close_log;	//是否关闭日志

};

#define LOG_DEBUG(format, ...) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif
