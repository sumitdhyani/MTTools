#pragma once
#include "Scheduler.hpp"

namespace ULMTTools
{
	class TaskScheduler final
	{
		typedef std::pair<time_point, Task> TimeTaskPair;
		typedef mtInternalUtils::Scheduler<Task> Scheduler;

		Scheduler m_timedConsumer;
	public:

		TaskScheduler() : m_timedConsumer([](Task task) {task(); })
		{}

		void push(const time_point& t, const Task& task)
		{
			m_timedConsumer.push(t, task);
		}
	};
	DEFINE_PTR(TaskScheduler)

}