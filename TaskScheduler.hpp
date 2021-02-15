#pragma once
#include "WorkerThread.hpp"

namespace mtTools
{
	class TaskScheduler
	{
		typedef std::pair<time_point, Task> TimeTaskPair;
		typedef mtInternalUtils::Scheduler<Task> Scheduler;
		DEFINE_UNIQUE_PTR(Scheduler)

			Scheduler m_timedConsumer;
	public:

		TaskScheduler() : m_timedConsumer([](Task task) {task(); })
		{}

		virtual void push(time_point t, Task task)
		{
			m_timedConsumer.push(t, task);
		}
	};
	DEFINE_PTR(TaskScheduler)

}