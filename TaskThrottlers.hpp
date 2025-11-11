#pragma once
#include "WorkerThread.hpp"
#include "TaskScheduler.hpp"

namespace ULMTTools
{
	class ThrottledWorkerThread
	{
		typedef mtInternalUtils::ThrottledConsumerThread<Task> ThrottledConsumerThread;
		ThrottledConsumerThread m_consumer;


	public:

		ThrottledWorkerThread(const duration& unitTime, const size_t& numTransactions)
			:m_consumer([](Task task) {task(); }, unitTime, numTransactions)
		{
		}

		void push(const Task& task)
		{
			m_consumer.push(task);
		}

		void kill()
		{
			m_consumer.kill();
		}
	};

	class ReusableThrottledWorkerThread
	{
		typedef mtInternalUtils::ReusableThrottler<Task> ReusableThrottler;
		ReusableThrottler m_throttler;
	public:
		ReusableThrottledWorkerThread(const std::shared_ptr<WorkerThread>& worker,
																	const std::shared_ptr<TaskScheduler> taskScheduler,
																	const duration& unitTime,
																	const size_t& numTransactions)
			:m_throttler([worker](const Task& task) { worker->push(task); },
										[taskScheduler](const time_point& scheduleTime, const Task& task) { taskScheduler->push(scheduleTime, task); },
										[](Task task) { task(); },
										unitTime,
										numTransactions)
		{
		}

		void push(const Task& task)
		{
			m_throttler.push(task);
		}
	};
}