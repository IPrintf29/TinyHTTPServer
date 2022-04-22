/*
 *log日志任务队列
 */

#ifndef LOG_QUEUE_H
#define LOG_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "locker.h"

using namespace std;

template<class T>
class log_queue {
public:
	//构造函数
	log_queue(int max_size = 1000) {
		if (max_size <= 0)
			exit(-1);

		m_max_size = max_size;
		m_array = new T[max_size];
		m_size = 0;
		m_front = -1;
		m_back = -1;
	}
	//析构函数
	~log_queue() {
		m_mutex.lock();
		if (m_array != nullptr)
			delete [] m_array;
		m_mutex.unlock();
	}

	//清空
	void clear() {
		m_mutex.lock();
		m_size = 0;
		m_front = -1;
		m_back = -1;
		m_mutex.unlock();
	}
	//判断队列是否为满
	bool full() {
		m_mutex.lock();
		if (m_size >= m_max_size) {
			m_mutex.unlock();
			return true;
		}
		m_mutex.unlock();
		return false;
	}
	//判断队列是否为空
	bool empty() {
		m_mutex.lock();
		if (m_size == 0) {
			m_mutex.unlock();
			return true;
		}
		m_mutex.unlock();
		return false;
	}
	//返回队首元素
	bool front(T &value) {
		m_mutex.lock();
		if (m_size == 0) {
			m_mutex.lock();
			return false;
		}
		value = m_array[m_front];
		m_mutex.unlock();
		return true;
	}
	//返回队尾元素
	bool back(T &value) {
		m_mutex.lock();
		if (m_size == 0) {
			m_mutex.lock();
			return false;
		}
		value = m_array[m_back];
		m_mutex.unlock();
		return true;
	}
	//返回当前元素个数
	int size() {
		int tmp = 0;
		m_mutex.lock();
		tmp = m_size;
		m_mutex.unlock();
		return tmp;
	}
	//返回最大元素个数
	int max_size() {
		return m_max_size;
	}
	//向队列中添加任务
	bool push(const T &item) {
		m_mutex.lock();
		if (m_size >= m_max_size) {
			m_cond.broadcast();
			m_mutex.unlock();
			return false;
		}

		//添加任务
		m_back = (m_back + 1) % m_max_size;
		m_array[m_back] = item;
		m_size++;

		m_cond.broadcast();
		m_mutex.unlock();
		return true;
	}
	//从队列中取出任务
	bool pop(T &item) {
		m_mutex.lock();
		//必须while，条件变量
		while (m_size <= 0) {
			if (!m_cond.wait(m_mutex.get())) {
				m_mutex.unlock();
				return false;
			}
		}

		//取出任务
		m_front = (m_front + 1) % m_max_size;
		item = m_array[m_front];
		m_size--;
		m_mutex.unlock();
		return true;
	}
private:
//成员数据
	locker m_mutex;	//互斥锁
	cond m_cond;	//条件变量

	T *m_array;	//任务数组
	int m_size;	//当前任务数量
	int m_max_size;	//数组最大容量
	int m_front;	//队列头指针
	int m_back;	//队列尾指针
};

#endif
