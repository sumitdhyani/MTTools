#pragma once
#include "WorkerThread.hpp"

namespace ULMTTools
{
	class ThreadPool
	{
		size_t m_numThreads;
		std::atomic<size_t> m_currWorkerIdx;
		WorkerThread* m_workers;

	public:
		ThreadPool(const uint& numThreads) : 
			m_numThreads(numThreads),
			m_currWorkerIdx(0),
			m_workers(new WorkerThread[numThreads])
		{
		}


		void push(const Task& task)
		{
			m_currWorkerIdx = (m_currWorkerIdx + 1) % m_numThreads;
			m_workers[m_currWorkerIdx].push(task);
		}

		void kill()
		{
			delete[] m_workers;
			m_workers = nullptr;
		}

		~ThreadPool()
		{
			kill();
		}
	};

}