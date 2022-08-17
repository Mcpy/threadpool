#pragma once
#include<thread>
#include<string>
#include<mutex>
#include<condition_variable>
#include<list>
#include<queue>
#include<atomic>
#include<iostream>

class ThreadTask
{
public:
	bool end_flag;
	bool error_flag;
	std::string error_msg;
	ThreadTask();
	virtual void start();
};

class ThreadPool
{
private:
	int core_pool_size;
	int max_pool_size;
	int buffer_size;
	int keep_alive_seconds;
	std::atomic<int> running_num;
	std::atomic<bool> termination_flag;
	std::condition_variable cv_thread_pool,cv_management;
	std::mutex mtx;
	std::queue<ThreadTask*> task_buffer;
	std::list<std::thread> thread_pool;
	std::thread threadpool_management;

	//处理多余线程需要的变量
	int last_running_num;
	std::atomic<bool> clear_flag;
	std::atomic<int> need_clear_num;
	std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;
	std::mutex mtx_clear_queue;
	std::queue<std::thread::id> clear_thread_id;


public:
	//core_pool_size 常驻线程数； max_pool_size 线程池最大线程数；buffer_size 任务缓存数量，当任务缓存数量大于此设定值后线程池中线程数将增长；keep_alive_seconds 多于常驻线程数量时，多余线程的存活的最短时间
	ThreadPool(int core_pool_size , int max_pool_size, int buffer_size, int  keep_alive_seconds);
	~ThreadPool();
	int poolSize();
	int bufferSize();
	void pushTask(ThreadTask* task);
	int runningNum();

private:
	ThreadPool(const ThreadPool& tp) = delete;
	ThreadPool& operator()(const ThreadPool& tp) = delete;
	void work();
	void threadpoolManagement();

};

