#ifndef TIME_WHEEL_TIMER
#define TIME_WHEEL_TIMER

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <mutex>

#define BUFFER_SIZE 64

class tw_timer;    

struct client_data{
	sockaddr_in address;     	
    int sockfd;              
	char buf[BUFFER_SIZE];
	tw_timer *timer;         
};

//定时器类
class tw_timer{

public:    //方法
	tw_timer( int rot, int ts )
	: next(nullptr), prev(nullptr), user_data(nullptr), rotation( rot ), time_slot( ts ){}

public:    
	int rotation;                     
	int time_slot;                    
	void (*cb_func)(client_data*);    
	client_data *user_data;           
	tw_timer *next;
	tw_timer *prev;
};

class time_wheel{

public:
	time_wheel() : cur_slot( 0 ){
		for (int i = 0; i < N; i++)
			slots[i] = nullptr;
	}
	~time_wheel(){
		for (int i = 0; i < N; i++){
			tw_timer *tmp = slots[i];
			while (tmp){
				slots[i] = tmp->next;
				delete tmp;
				tmp = slots[i];
			}
		}
	}

	tw_timer *add_timer( int timeout ){
		if ( timeout < 0)
			return nullptr;
		int ticks = 0;
		if ( timeout < SI )
			ticks = 1;
		else
			ticks = timeout / SI;

		int rotation = ticks / N;
		int ts = ( cur_slot + ( ticks % N ) ) % N;
		tw_timer *timer = new tw_timer( rotation, ts);

		//对槽上锁
		std::lock_guard<std::mutex> lock(mutexslots[ts]);
		if ( !slots[ts] )
			slots[ts] = timer;
		else {
			timer->next = slots[ts];
			slots[ts]->prev = timer;
			slots[ts] = timer;
		}

		return timer;
	}

	void del_timer( tw_timer *&timer ){
		if ( !timer )
			return;

		int ts = timer->time_slot;
		//对槽上锁
		std::lock_guard<std::mutex> lock(mutexslots[ts]);
		if ( timer == slots[ts] ){             
			slots[ts] = timer->next;
			if ( slots[ts] )
				slots[ts]->prev = nullptr;
			delete timer;
			timer = nullptr;
		}
		else {
			timer->prev->next = timer->next;
			if ( timer->next )
				timer->next->prev = timer->prev;
			delete timer;
			timer = nullptr;
		}
	}

	void adjust_timer( tw_timer *&timer, int timeout ){
	/*
	#ifdef OLD_VERSION
		if ( timeout < 0)
			return;
		int ticks = 0;
		if ( timeout < SI )
			ticks = 1;
		else
			ticks = timeout / SI;
		int rotation = ticks / N;
		int ts = ( cur_slot + ( ticks % N ) ) % N;
		int old_ts = timer->time_slot;
		timer->rotation = rotation;
		if ( ts == old_ts )
			return;
		timer->time_slot = ts;

		if ( timer == slots[old_ts] ){             
			slots[old_ts] = timer->next;
			if ( slots[old_ts] )
				slots[old_ts]->prev = nullptr;
		}
		else {
			timer->prev->next = timer->next;
			if ( timer->next )
				timer->next->prev = timer->prev;
		}

		
		if ( !slots[ts] )
			slots[ts] = timer;
		else {
			timer->next = slots[ts];
			slots[ts]->prev = timer;
			slots[ts] = timer;
		}
	#endif
	*/
		int ts = timer->time_slot;
		//对槽上锁
		std::lock_guard<std::mutex> lock(mutexslots[ts]);
		if (timer->rotation == 0)
			++timer->rotation;
	}

	void tick(){
		//对槽上锁
		std::lock_guard<std::mutex> lock(mutexslots[cur_slot]);
		tw_timer *tmp = slots[cur_slot];

		while ( tmp ){
			if (tmp->rotation > 0){          
				tmp->rotation--;
				tmp = tmp->next;
			}
			else {
				tmp->cb_func( tmp->user_data );
				if ( tmp == slots[cur_slot] ){
					slots[cur_slot] = tmp->next;
					if ( slots[cur_slot] )
						slots[cur_slot]->prev = nullptr;
					delete tmp;
					tmp = slots[cur_slot];
				}
				else {
					tmp->prev->next = tmp->next;
					if ( tmp->next )
						tmp->next->prev = tmp->prev;
					tw_timer *tmp2 = tmp->next;
					delete tmp;
					tmp = tmp2;
				}
			}
		}

		cur_slot = (++cur_slot) % N;    
	}

private:
	static const int N = 60;       
	static const int SI = 1;       
	tw_timer *slots[N];            
	//一个槽对应一个锁
	std::mutex mutexslots[N];
	int cur_slot;                  
};

#endif
