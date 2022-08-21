#pragma once
#ifndef _CPY_THREAD_POOL_H
#define _CPY_THREAD_POOL_H
#include<thread>
#include<string>
#include<mutex>
#include<condition_variable>
#include<list>
#include<queue>
#include<atomic>
#include<iostream>
#include<future>

class ThreadPool
{
private:
	using Task = std::function<void()>;
	unsigned short core_pool_size;
	unsigned short max_pool_size;
	unsigned short buffer_size;
	unsigned int keep_alive_seconds;
	std::atomic<int> running_num;
	std::atomic<bool> termination_flag;
	std::condition_variable cv_thread_pool,cv_management;
	std::mutex mtx;
	std::queue<Task> task_buffer;
	std::list<std::thread> thread_pool;
	std::thread threadpool_management;

	//��������߳���Ҫ�ı���
	int last_running_num;
	std::atomic<bool> clear_flag;
	std::atomic<int> need_clear_num;
	std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;
	std::mutex mtx_clear_queue;
	std::queue<std::thread::id> clear_thread_id;


public:
	//core_pool_size ��פ�߳����� max_pool_size �̳߳�����߳�����buffer_size ���񻺴������������񻺴��������ڴ��趨ֵ���̳߳����߳�����������keep_alive_seconds ���ڳ�פ�߳�����ʱ�������̵߳Ĵ������ʱ��
	ThreadPool(unsigned short core_pool_size , unsigned short max_pool_size, unsigned short buffer_size, unsigned int  keep_alive_seconds);
	~ThreadPool();
	int poolSize() const;
	int bufferSize() const;
	template<class Func, class... Args>
	auto pushTask(Func&& func, Args&&... args)->std::future<decltype(func(args...)) >;
	template<class Func, class ObjPtr, class... Args>
	auto pushTask(Func&& func, ObjPtr&& objptr, Args&&... args)->std::future<decltype((objptr->*func)(args...))>;
	int runningNum() const;
	void close();

private:
	ThreadPool(const ThreadPool& tp) = delete;
	ThreadPool& operator()(const ThreadPool& tp) = delete;
	void work();
	void threadpoolManagement();

};

template<class Func, class ...Args>
auto ThreadPool::pushTask(Func&& func, Args && ...args)->std::future<decltype(func(args...)) >
{
	if (termination_flag)
		throw("ThreadPool::pushTask: This threadpool has been closed!");
	using ReturnType = decltype(func(args...));
	//����ָ�룻������������
	auto p_task = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
	std::future<ReturnType> future = p_task->get_future();
	std::unique_lock<std::mutex> ulock(mtx);
	task_buffer.emplace([p_task]() {(*p_task)(); });
	ulock.unlock();
	cv_management.notify_one();
	return future;
}

template<class Func, class ObjPtr, class ...Args>
auto ThreadPool::pushTask(Func&& func, ObjPtr&& objptr, Args && ...args) -> std::future<decltype((objptr->*func)(args ...))>
{
	if (termination_flag)
		throw("ThreadPool::pushTask: This threadpool has been closed!");
	using ReturnType = decltype((objptr->*func)(args ...));
	auto p_task = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<Func>(func), std::forward<ObjPtr>(objptr), std::forward<Args>(args)...));
	std::future<ReturnType> future = p_task->get_future();
	std::unique_lock<std::mutex> ulock(mtx);
	task_buffer.emplace([p_task]() {(*p_task)(); });
	ulock.unlock();
	cv_management.notify_one();
	return future;
}
#endif 
