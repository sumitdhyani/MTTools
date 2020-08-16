#pragma once
#include "ConsumerThread.h"
#include <CommonUtils/CommonDefs.h>
namespace ULMTTools
{
	typedef IConsumerThread<Task> IWorkerThread;
	typedef ISuspendableConsumerThread<Task> ISuspendableWorkerThread;

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
		{
			m_consumer = std::make_unique<ThrottledConsumerThread>(queue, executor, unitTime, numTransactions);
		}

	public:
		ThrottledWorkerThread(duration unitTime, size_t numTransactions, std::function<void(Task)> executor = [](Task task) {task();})
			:ThrottledWorkerThread(std::make_shared<std::vector<Task>>(), unitTime, numTransactions)
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

	typedef std::pair<std::chrono::system_clock::time_point, Task> TimedTask;

	typedef std::pair<std::chrono::system_clock::time_point, Task> TimeTaskPair;
	class Scheduler : public ISchedulerConsumerThread<Task>
	{
		typedef TimedConsumerThread<Task> TimedConsumerThread;
		DEFINE_UNIQUE_PTR(TimedConsumerThread)

		TimedConsumerThread_UPtr m_timedConsumer;
		
		Scheduler(std::shared_ptr<std::vector<TimeTaskPair>> queue, stdMutex_SPtr mutex, ConditionVariable_SPtr cond, std::function<void(Task)> executor = [](Task task) {task(); })
			:m_timedConsumer(std::make_unique<TimedConsumerThread>(queue, mutex, executor, cond))
		{}
	public:
		explicit Scheduler(std::function<void(Task)> executor = [](Task task) {task(); }) :
			Scheduler(std::make_shared<std::vector<TimeTaskPair>>(), std::make_shared<std::mutex>(), std::make_shared<ConditionVariable>(), executor)
		{}
	};
	DEFINE_PTR(Scheduler)
}
