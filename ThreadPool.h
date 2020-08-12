#pragma once
#include "WorkerThread.h"

class IThreadPool
{
public:
	virtual void push(Task task) = 0;
	virtual void kill() = 0;
	virtual ~IThreadPool() {}
};

class ThreadPool
{
	std::vector<std::shared_ptr<WorkerThread>> m_workers;
	std::shared_ptr<std::vector<Task>> m_queue;
	stdMutex_SPtr m_mutex;
	ConditionVariable_SPtr m_cond;

protected:
	ThreadPool(UINT numThreads, std::shared_ptr<std::vector<Task>> queue, stdMutex_SPtr mutex, ConditionVariable_SPtr cond)
		:m_queue(queue),
		m_mutex(mutex),
		m_cond(cond)
	{
		for (UINT i = 0; i < numThreads; i++)
		{
			m_workers.push_back(std::shared_ptr<WorkerThread>(new WorkerThread(queue, mutex, cond)));
		}
	}

public:
	ThreadPool(UINT numThreads, std::shared_ptr<std::vector<Task>> queue, stdMutex_SPtr mutex):
		ThreadPool(numThreads, queue, mutex, ConditionVariable_SPtr(new ConditionVariable))
	{
	}

	virtual void push(Task task)
	{
		{
			stdUniqueLock lock(*m_mutex);
			m_queue->push_back(task);
		}

		m_cond->notify_one();
	}

	virtual void kill()
	{
		for (auto& worker : m_workers)
			worker->kill();
	}

	~ThreadPool()
	{
		kill();
	}
};

