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
		//�����ǣ��������߳�
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
		//������������������񻺴�����Ҫ����߳�
		if (task_num - free_thread > (int)buffer_size)
		{
			int add_num = task_num-free_thread;
			//����������ó�������߳���
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
		//��������񻺴�������Ӧ�������߳�
		for (int i = 0; i < task_num; i++)
		{
			cv_thread_pool.notify_one();
		}
		//�����ǰ���߳��������˳�פ�߳�����ʱ����Ҫ���������߳�
		if (thread_pool.size() > core_pool_size)
		{
			//��û�����������û��Ҫ������߳�ʱ
			if (!clear_flag && clear_thread_id.empty())
			{
				//ÿkeep_alive_seconds���һ��
				if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - timestamp).count() > keep_alive_seconds)
				{
					timestamp = std::chrono::high_resolution_clock::now();
					//�����ڹ������߳̽��ϸ�ʱ����٣�����ΪĿǰ�����ڼ�С������������ҿ��е��߳�
					//Ҳ�ɸ�Ϊ last_running_num - running_num > a aΪһ����ֵ
					if (running_num < last_running_num)
					{
						int free_thread = thread_pool.size() - running_num, extra_thread = thread_pool.size() - core_pool_size;
						need_clear_num = free_thread < extra_thread ? free_thread : extra_thread;
						last_running_num = running_num;
						if (need_clear_num > 0)
						{
							clear_flag = 1;
							//������Ҫ������߳�����
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
		//ɾ���Ѿ��������߳�
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


