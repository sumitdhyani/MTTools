#pragma once
#include "ConsumerThread.hpp"
#include "CommonUtils/CommonDefs.hpp"

namespace ULMTTools
{
	class WorkerThread
	{
		friend class ThreadPool;
		typedef mtInternalUtils::FifoConsumerThread<Task> Consumer;
		typedef Consumer::ConsumerQueue TaskQueue;
		DEFINE_PTR(TaskQueue)

			Consumer m_consumer;

	public:
		WorkerThread()
			:m_consumer([](Task task) {task(); })
		{
		}

		void push(const Task& task)
		{
			m_consumer.push(task);
		}

		//returns number of pending tasks 
		size_t size()
		{
			return m_consumer.size();
		}

		void kill()
		{
			m_consumer.kill();
		}
	};
	DEFINE_PTR(WorkerThread)
}
