#pragma once
#include "ConsumerThread.hpp"
#include "CommonUtils/CommonDefs.hpp"

namespace mtTools
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

		void push(Task task)
		{
			m_consumer.push(task);
		}

		void pause()
		{
			m_consumer.pause();
		}

		void resume()
		{
			m_consumer.resume();
		}

		//returns number of pending tasks 
		size_t size()
		{
			return m_consumer.size();
		}
	};
	DEFINE_PTR(WorkerThread)
}
