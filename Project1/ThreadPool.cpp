#include "ThreadPool.h"



ThreadPool::ThreadPool(unsigned short core_pool_size, unsigned short max_pool_size, unsigned short buffer_size, unsigned int keep_alive_seconds)
	:core_pool_size(core_pool_size), max_pool_size(max_pool_size), buffer_size(buffer_size), keep_alive_seconds(keep_alive_seconds), termination_flag(0), running_num(0),
	last_running_num(0), clear_flag(0), need_clear_num(0), timestamp(std::chrono::high_resolution_clock::now()), threadpool_management(&ThreadPool::threadpoolManagement, this)
{
	try
	{
		for (int i = 0; i < core_pool_size; i++)
		{
			thread_pool.emplace_back(&ThreadPool::work, this);
		}
	}
	catch (...)
	{
		close();
		throw;
	}
}

ThreadPool::~ThreadPool()
{
	close();
}

int ThreadPool::poolSize() const
{
	return thread_pool.size();
}

int ThreadPool::bufferSize() const
{
	return task_buffer.size();
}

int ThreadPool::runningNum() const
{
	return running_num;
}

void ThreadPool::close()
{
	termination_flag = 1;
	cv_management.notify_one();
	if (threadpool_management.joinable())
		threadpool_management.join();
	cv_thread_pool.notify_all();
	for (auto& i : thread_pool)
	{
		if (i.joinable())
		{
			i.join();
		}
	}
}

void ThreadPool::work()
{
	while (!termination_flag)
	{
		std::unique_lock<std::mutex> ulock(mtx);
		while (task_buffer.empty() && !clear_flag && !termination_flag)
		{
			cv_thread_pool.wait(ulock);
		}
		if (termination_flag)
			break;
		//清理标记，结束本线程
		if (clear_flag)
		{
			if (need_clear_num > 0)
			{
				ulock.unlock();
				need_clear_num--;
				std::unique_lock<std::mutex> ulock_clear_queue(mtx_clear_queue);
				clear_thread_id.push(std::this_thread::get_id());
				break;
			}
			else
			{
				clear_flag = 0;
			}
		}
		Task task = std::move(task_buffer.front());
		task_buffer.pop();
		ulock.unlock();
		running_num++;
		task();
		running_num--;
	}
}

void ThreadPool::threadpoolManagement()
{
	while (!termination_flag)
	{
		std::unique_lock<std::mutex> ulock(mtx);
		while (task_buffer.empty() && thread_pool.size() <= core_pool_size && !termination_flag)
		{
			cv_management.wait(ulock);
		}
		if (termination_flag)
			break;
		int task_num = task_buffer.size();
		ulock.unlock();
		int free_thread = thread_pool.size() - running_num;
		//如果任务量超过了任务缓存则需要添加线程
		if (task_num - free_thread > (int)buffer_size)
		{
			int add_num = task_num-free_thread;
			//添加数量不得超过最大线程数
			if (add_num + thread_pool.size() > max_pool_size)
			{
				add_num = max_pool_size - thread_pool.size();
			}
			for (int i = 0; i < add_num; i++)
			{
				thread_pool.emplace_back(&ThreadPool::work, this);
			}
			timestamp = std::chrono::high_resolution_clock::now();
			last_running_num = running_num;
		}
		//如果有任务缓存则唤醒相应数量的线程
		for (int i = 0; i < task_num; i++)
		{
			cv_thread_pool.notify_one();
		}
		//如果当前的线程数大于了常驻线程数量时则需要处理多余的线程
		if (thread_pool.size() > core_pool_size)
		{
			//且没有清理任务和没有要清理的线程时
			if (!clear_flag && clear_thread_id.empty())
			{
				//每keep_alive_seconds检测一次
				if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - timestamp).count() > keep_alive_seconds)
				{
					timestamp = std::chrono::high_resolution_clock::now();
					//当正在工作的线程较上个时间段少，可认为目前负载在减小，可清除多余且空闲的线程
					//也可改为 last_running_num - running_num > a a为一个阈值
					if (running_num < last_running_num)
					{
						int free_thread = thread_pool.size() - running_num, extra_thread = thread_pool.size() - core_pool_size;
						need_clear_num = free_thread < extra_thread ? free_thread : extra_thread;
						last_running_num = running_num;
						if (need_clear_num > 0)
						{
							clear_flag = 1;
							//唤醒需要清理的线程数量
							for (int i = 0; i < need_clear_num; i++)
							{
								cv_thread_pool.notify_one();
							}
						}
					}
				}
			}
			std::this_thread::yield();
		}
		//删除已经结束的线程
		if (!clear_thread_id.empty())
		{
			std::thread::id thread_id;
			std::unique_lock<std::mutex> ulock_clear_queue(mtx_clear_queue);
			thread_id = clear_thread_id.front();
			clear_thread_id.pop();
			ulock_clear_queue.unlock();
			for (auto i = thread_pool.begin(); i != thread_pool.end(); i++)
			{
				if (i->get_id() == thread_id)
				{
					if (i->joinable())
						i->join();
					thread_pool.erase(i);
					break;
				}
			}
		}
	}
}


