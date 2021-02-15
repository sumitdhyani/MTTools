#pragma once
#include "WorkerThread.hpp"
#include "TaskScheduler.hpp"

namespace ULMTTools
{
	class ThrottledWorkerThread
	{
		typedef mtInternalUtils::ThrottledConsumerThread<Task> ThrottledConsumerThread;
		DEFINE_UNIQUE_PTR(ThrottledConsumerThread)

			ThrottledConsumerThread_UPtr m_consumer;


	public:
#if defined(ENABLE_MTTOOLS_TESTING)//This constructor is for testing purpose only, not available to the client code
		ThrottledWorkerThread(std::shared_ptr<std::vector<Task>> queue, duration unitTime, size_t numTransactions, std::function<void(Task)> executor = [](Task task) {task(); })
			:m_consumer(std::make_unique<ThrottledConsumerThread>(queue, executor, unitTime, numTransactions))
		{
		}
#endif //ENABLE_MTTOOLS_TESTSING

		ThrottledWorkerThread(duration unitTime, size_t numTransactions)
			:m_consumer(std::make_unique<ThrottledConsumerThread>(std::make_shared<std::vector<Task>>(), [](Task task) {task(); }, unitTime, numTransactions))
		{
		}

		void push(Task task)
		{
			m_consumer->push(task);
		}

		void kill()
		{
			m_consumer->kill();
		}
	};

	class ReusableThrottledWorkerThread
	{
		typedef mtInternalUtils::ReusableThrottler<Task> ReusableThrottler;
		DEFINE_UNIQUE_PTR(ReusableThrottler)

			ReusableThrottler_UPtr m_consumer;
	public:
		ReusableThrottledWorkerThread(std::shared_ptr<WorkerThread> worker, std::shared_ptr<TaskScheduler> taskScheduler, duration unitTime, size_t numTransactions)
			:m_consumer(std::make_unique<ReusableThrottler>(worker, taskScheduler, [](Task task) {task(); }, unitTime, numTransactions))
		{
		}

		void push(Task task)
		{
			m_consumer->push(task);
		}
	};
	DEFINE_PTR(ReusableThrottledWorkerThread)
}