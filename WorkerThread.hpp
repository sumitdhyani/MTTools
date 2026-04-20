#pragma once
#include "ConsumerThread.hpp"
#include "CommonUtils/CommonDefs.hpp"

namespace ULMTTools
{
	class WorkerThread
	{
		friend class ThreadPool;
		typedef mtInternalUtils::FifoConsumerThread<Task> Consumer;
		Consumer m_consumer;

	public:
		WorkerThread()
			:m_consumer([](Task task) {task(); })
		{
		}

		bool push(const Task& task)
		{
			return m_consumer.push(task);
		}

		bool push(Task&& task)
		{
			return m_consumer.push(std::move(task));
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
}
