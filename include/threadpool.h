#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "locker.h"
#include "sql_connection_pool.h"

//线程池类，模板参数T是任务类
template < typename T >
class threadpool {
	public:
		//thread_num当前线程数量，max_requests最多允许的，等待的请求数量
		threadpool( int thread_num = 4, int max_requests = 8000 );
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
		//请求队列
		std::list< T* > m_workqueue;
	#ifdef _CPLUSPLUS11
		//保护请求队列的互斥锁
		mutex m_queuelocker;
		//信号量
		condition_variable m_queuestat_full;
		//程线程池数组，元素为线程
		vector<thread*> m_threads;
	#else
		locker m_queuelocker;
		sem m_queuestat_empty;
		sem m_queuestat_full;
		pthread_t *m_threads;
	#endif
		//是否结束线程
		bool m_stop;
};

template < typename T >
threadpool< T >::threadpool( int thread_number, int max_requests ) :
	m_thread_number( thread_number ), m_max_requests( max_requests ), 
	m_stop( false ), m_threads( NULL )
	#ifndef _CPLUSPLUS11
	, m_queuestat_empty(max_requests), m_queuestat_full(0) 
	#endif
	{
		if ( ( thread_number <= 0 ) || ( max_requests <= 0 ) )
			throw std::exception();
		//动态创建线程池
	#ifndef _CPLUSPLUS11
		// pthread version
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
	#else
		for (int i = 0; i < thread_number; ++i) {
			printf( "create the %dth thread\n", i );
			m_threads.emplace_back(new thread(worker, this));

			m_threads[i]->detach();
		}
	#endif

		//数据库指向初始为nullptr
		m_connection_pool = nullptr;
	}

template < typename T >
threadpool< T >::~threadpool() {
	#ifndef _CPLUSPLUS11
	//pthread version
	delete [] m_threads;
	#else
	for (auto &c: m_threads)
		delete c;
	#endif
	m_stop = true;
}

//往请求队列添加任务
template < typename T >
bool threadpool< T >::append( T *request ) {
	#ifndef _CPLUSPLUS11
	// pthread version
	m_queuestat_empty.wait();
	m_queuelocker.lock();

	if ( m_workqueue.size() > m_max_requests ) {
		m_queuelocker.unlock();
		return false;
	}
	m_workqueue.push_back( request );

	m_queuelocker.unlock();
	m_queuestat_full.post();
	#else
	unique_lock<mutex> lock(m_queuelocker);

	if ( m_workqueue.size() > m_max_requests ) {
		lock.unlock();
		return false;
	}
	m_workqueue.push_back(request);

	m_queuestat_full.notify_all();
	#endif
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
	#ifndef _CPLUSPLUS11
	// pthread_version
	while ( !m_stop ) {
		m_queuestat_full.wait();
		m_queuelocker.lock();

		if ( m_workqueue.empty() ) {
			m_queuelocker.unlock();
			continue;
		}

		T *request = m_workqueue.front();
		m_workqueue.pop_front();

		m_queuelocker.unlock();
		m_queuestat_empty.post();
		//如果沒有取出任務
		if ( !request )
			continue;
		//request获得一个数据库连接
		//connectionRAII mysqlcon(&request->m_mysql, m_connection_pool);
		request->process();
	}
	#else
	while ( !m_stop ) {
		unique_lock<mutex> lock(m_queuelocker);

		while (m_workqueue.empty())
			m_queuestat_full.wait(lock);

		T *request = m_workqueue.front();
		m_workqueue.pop_front();

		lock.unlock();
		m_queuestat_full.notify_all();
		//如果沒有取出任務
		if ( !request )
			continue;
		//request获得一个数据库连接
		//connectionRAII mysqlcon(&request->m_mysql, m_connection_pool);
		request->process();
	}
	#endif
}

#endif
