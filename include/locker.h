#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//信号量类
class sem {
public:
	//创建并初始化信号量
	sem(){
		if ( sem_init( &m_sem, 0, 0 ) != 0)
			//构造函数无返回值，通过拋出异常报告错误
			throw std::exception();
	}
	sem(int num) {
		if (sem_init(&m_sem, 0, num) != 0)
			throw std::exception();
	}

	//销毁信号量
	~sem(){
		sem_destroy( &m_sem );
	}

	//等待信号量
	bool wait(){
		return sem_wait( &m_sem ) == 0;
	}

	//释放信号量
	bool post(){
		return sem_post( &m_sem ) == 0;
	}

private:
	sem_t m_sem;
	
};

//互斥锁类
class locker {
public:
	//创建互斥锁并初始化
	locker() {
		if ( pthread_mutex_init( &m_mutex, NULL ) != 0 )
			throw std::exception();
	}

	//销毁互斥锁
	~locker() {
		pthread_mutex_destroy( &m_mutex );
	}

	//获取互斥锁
	bool lock() {
		return pthread_mutex_lock( &m_mutex ) == 0;
	}

	//释放互斥锁
	bool unlock() {
		return pthread_mutex_unlock( &m_mutex ) == 0;
	}
	//获取互斥锁内部对象，用于条件变量
	pthread_mutex_t *get() {
		return &m_mutex;
	}
private:
	pthread_mutex_t m_mutex;

};

//条件变量类，需要和互斥锁一起使用
class cond {
public:
	//创建条件变量并初始化
	cond() {
		if ( pthread_cond_init( &m_cond, NULL ) != 0 )
			throw std::exception();
	}

	//销毁条件变量
	~cond() {
		pthread_cond_destroy( &m_cond );
	}

	//等待条件变量
	bool wait(pthread_mutex_t *m_mutex) {
		int ret = 0;
		ret = pthread_cond_wait( &m_cond, m_mutex );
		return ret == 0;
	}

	//唤醒等待条件变量的线程
	bool signal() {
		return pthread_cond_signal( &m_cond ) == 0;
	}
	//广播
	bool broadcast() {
		return pthread_cond_broadcast(&m_cond) == 0;
	}

private:
	pthread_cond_t m_cond;
};

#endif
