#pragma once
#include "ConsumerThread.hpp"
#include <CommonUtils/CommonDefs.hpp>
namespace ULMTTools
{
	typedef IConsumerThread<Task> IWorkerThread;
	typedef ISuspendableConsumerThread<Task> ISuspendableWorkerThread;
	typedef ISchedulerConsumerThread<Task> ITaskScheduler;

	class WorkerThread : public ISuspendableWorkerThread
	{
		friend class ThreadPool;
		std::shared_ptr<FifoConsumerThread<Task>> m_consumer;
	protected:


		WorkerThread(std::shared_ptr<std::vector<Task>> queue, stdMutex_SPtr mutex, ConditionVariable_SPtr cond, std::function<void(Task)> executor = [](Task task) {task(); })
		{
			m_consumer = std::make_shared<FifoConsumerThread<Task>>(queue, mutex, executor);
		}

	public:
		explicit WorkerThread(std::function<void(Task)> executor = [](Task task) {task(); }) :
			WorkerThread(std::make_shared<std::vector<Task>>(), std::make_shared<stdMutex>(), std::make_shared<ConditionVariable>(), executor)
		{}

		void push(Task task)
		{
			m_consumer->push(task);
		}

		void kill()
		{
			m_consumer->kill();
		}

		void pause()
		{
			m_consumer->pause();
		}

		void resume()
		{
			m_consumer->resume();
		}

	};
	DEFINE_PTR(WorkerThread)


	class ThrottledWorkerThread : IWorkerThread
	{
		typedef ThrottledConsumerThread<Task> ThrottledConsumerThread;
		DEFINE_UNIQUE_PTR(ThrottledConsumerThread)

		ThrottledConsumerThread_UPtr m_consumer;

		ThrottledWorkerThread(std::shared_ptr<std::vector<Task>> queue, duration unitTime, size_t numTransactions, std::function<void(Task)> executor = [](Task task) {task(); })
			:m_consumer(std::make_unique<ThrottledConsumerThread>(queue, executor, unitTime, numTransactions))
		{
		}

	public:
		ThrottledWorkerThread(duration unitTime, size_t numTransactions, std::function<void(Task)> executor = [](Task task) {task();})
			:ThrottledWorkerThread(std::make_shared<std::vector<Task>>(), unitTime, numTransactions, executor)
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

	class TaskScheduler : public ITaskScheduler
	{
		typedef std::pair<std::chrono::system_clock::time_point, Task> TimeTaskPair;
		typedef TimedConsumerThread<Task> TimedConsumerThread;
		DEFINE_UNIQUE_PTR(TimedConsumerThread)

		TimedConsumerThread_UPtr m_timedConsumer;
		
		TaskScheduler(std::shared_ptr<std::vector<TimeTaskPair>> queue, stdMutex_SPtr mutex, ConditionVariable_SPtr cond, std::function<void(Task)> executor = [](Task task) {task(); })
			:m_timedConsumer(std::make_unique<TimedConsumerThread>(queue, mutex, executor, cond))
		{}
	public:
		explicit TaskScheduler(std::function<void(Task)> executor = [](Task task) {task(); }) :
			TaskScheduler(std::make_shared<std::vector<TimeTaskPair>>(), std::make_shared<std::mutex>(), std::make_shared<ConditionVariable>(), executor)
		{}

		virtual void push(time_point t, Task task)
		{
			m_timedConsumer->push(t, task);
		}

		virtual void kill()
		{
			m_timedConsumer->kill();
		}
	};
	DEFINE_PTR(TaskScheduler)

	class ReusableThrottledWorkerThread : IWorkerThread
	{
		typedef ReusableThrottler<Task> ReusableThrottler;
		DEFINE_UNIQUE_PTR(ReusableThrottler)

		ReusableThrottler_UPtr m_consumer;
	public:
		ReusableThrottledWorkerThread(std::shared_ptr<IWorkerThread> worker, std::shared_ptr<ITaskScheduler> taskScheduler, duration unitTime, size_t numTransactions, std::function<void(Task)> executor = [](Task task) {task(); })
			:m_consumer(std::make_unique<ReusableThrottler>(worker, taskScheduler, executor, unitTime, numTransactions))
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
	DEFINE_PTR(ReusableThrottledWorkerThread)
}
