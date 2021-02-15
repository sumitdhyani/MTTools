#pragma once
#include "WorkerThread.hpp"

namespace ULMTTools
{
	class ThreadPool
	{
		typedef WorkerThread::TaskQueue TaskQueue;
		DEFINE_PTR(TaskQueue)
			TaskQueue m_queue;
		stdMutex m_mutex;
		mtInternalUtils::ConditionVariable m_cond;
		std::atomic<bool> m_running;
		bool m_consumerBusy;
		stdThread m_thread;

		void run(size_t numWorkers)
		{
			std::vector<std::unique_ptr<WorkerThread>> workers;
			for (size_t i = 0; i < numWorkers; i++)
				workers.push_back(std::make_unique<WorkerThread>());

			while (m_running)
			{
				TaskQueue local;

				{
					stdUniqueLock lock(m_mutex);

					if (m_queue.empty())
					{
						m_consumerBusy = false;
						m_cond.wait(lock);
					}

					m_queue.swap(local);
					m_consumerBusy = true;
				}

				for (auto i = 0; i < local.size(); i++)
					workers[i % workers.size()]->push(local[i]);
			}

			{
				stdUniqueLock lock(m_mutex);
				if (!m_queue.empty())
				{
					TaskQueue local;
					m_queue.swap(local);
					lock.unlock();
					size_t index = 0;
					for (auto it = local.begin(); it != local.end(); it++)
						workers[index++ % numWorkers]->push(*it);
				}
			}
		}

		void kill()
		{
			stdUniqueLock lock(m_mutex);
			if (m_running)
			{
				m_running = false;
				lock.unlock();
				m_cond.notify_one();
				m_thread.join();
			}
		}

	public:
		ThreadPool(uint numThreads)
		{
			numThreads = std::max<size_t>(2, numThreads);
			m_thread = std::thread(std::bind(&ThreadPool::run, this, numThreads - 1));
			m_running = true;
			m_consumerBusy = false;
		}


		void push(Task task)
		{
			{
				stdUniqueLock lock(m_mutex);
				m_queue.push_back(task);
				if (!m_consumerBusy)
				{
					lock.unlock();
					m_cond.notify_one();
				}
			}
		}

		~ThreadPool()
		{
			kill();
		}
	};

}