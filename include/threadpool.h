#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include "locker.h"
#include "sql_connection_pool.h"

//线程池类，模板参数T是任务类
template < typename T >
class threadpool {
	public:
		//thread_num当前线程数量，max_requests最多允许的，等待的请求数量
		threadpool( int thread_num = 8, int max_requests = 10000 );
		~threadpool();
		//往请求队列添加任务
		bool append( T *request );

		//数据库指向
		connection_pool *m_connection_pool;

	private:
		//工作线程中运行的函数
		//定义为静态函数，不依赖与类对象是否存在
		static void *worker( void *arg );
		void run();

	private:
		//线程池中当前线程数量
		int m_thread_number;
		//请求队列中允许的最大请求数量
		int m_max_requests;
		//程线程池数组，元素为线程
		pthread_t *m_threads;
		//请求队列
		std::list< T* > m_workqueue;
		//保护请求队列的互斥锁
		locker m_queuelocker;
		//信号量
		sem m_queuestat;
		//是否结束线程
		bool m_stop;
};

template < typename T >
threadpool< T >::threadpool( int thread_number, int max_requests ) :
	m_thread_number( thread_number ), m_max_requests( max_requests ), 
	m_stop( false ), m_threads( NULL ) {
		if ( ( thread_number <= 0 ) || ( max_requests <= 0 ) )
			throw std::exception();
		//动态创建线程池
		m_threads = new pthread_t[ m_thread_number ];
		if ( !m_threads )
			throw std::exception();

		for ( int i = 0; i < thread_number; ++i) {
			printf( "create the %dth thread\n", i );
			//及其重要
			//C++ pthread_create中第三个参数必须为静态成员函数
			//参数传递则将线程池对象传给worker函数
			//最后再次调用线程池对象的函数方法
			if ( pthread_create( m_threads + i, NULL, worker, this ) 
					!= 0 ) {
				delete [] m_threads;
				throw std::exception();
			}
			//线程分离
			if ( pthread_detach( m_threads[i] ) ) {
				delete [] m_threads;
				throw std::exception();
			}
		}

		//数据库指向初始为nullptr
		m_connection_pool = nullptr;
	}

template < typename T >
threadpool< T >::~threadpool() {
	delete [] m_threads;
	m_stop = true;
}

//往请求队列添加任务
template < typename T >
bool threadpool< T >::append( T *request ) {
	m_queuelocker.lock();

	if ( m_workqueue.size() > m_max_requests ) {
		m_queuelocker.unlock();
		return false;
	}
	m_workqueue.push_back( request );

	m_queuelocker.unlock();
	m_queuestat.post();
	return true;
}

//每个线程执行的函数
template < typename T >
void *threadpool< T >::worker( void *arg ) {
	threadpool *pool = ( threadpool *)arg;
	//执行run函数
	pool->run();
	return pool;
}

//线程实际执行的函数 run函数
template < typename T >
void threadpool< T >::run() {
	while ( !m_stop ) {
		m_queuestat.wait();
		m_queuelocker.lock();

		if ( m_workqueue.empty() ) {
			m_queuelocker.unlock();
			continue;
		}

		T *request = m_workqueue.front();
		m_workqueue.pop_front();

		m_queuelocker.unlock();
		//如果沒有取出任務
		if ( !request )
			continue;
		//request获得一个数据库连接
		connectionRAII mysqlcon(&request->m_mysql, m_connection_pool);
		request->process();
	}
}

#endif
