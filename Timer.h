#pragma once
#include "WorkerThread.h"
#include <unordered_map>

class ITimer
{
public:
	virtual size_t install(Task task, duration interval) = 0;
	virtual void unInstall(size_t timerId) = 0;

	virtual ~ITimer() {}
};
DEFINE_PTR(ITimer)
DEFINE_UNIQUE_PTR(ITimer)


//Pass "default" to get the production implementation
class ITimerFactory
{
public:
	virtual ITimer_UPtr create(std::string type) = 0;

	virtual ~ITimerFactory() {}
};

class Timer : public ITimer
{
	typedef std::pair<Task, duration> TaskDurationPair;

	TimedTaskWorkerThread_UPtr m_workerThread;
	std::unordered_map<size_t, TaskDurationPair> m_taskListByTimerID;
	stdMutex m_mutex;
public:
	Timer(TimedTaskWorkerThread_UPtr workerThread)
	{
		m_workerThread = std::move(workerThread);
	}

	size_t install(Task task, duration interval)
	{
		auto timerId = std::hash<long long>()(std::chrono::system_clock::now().time_since_epoch().count() + rand());

		{
			std::unique_lock<stdMutex> lock(m_mutex);
			while(m_taskListByTimerID.find(timerId) != m_taskListByTimerID.end())
				timerId = std::hash<long long>()(std::chrono::system_clock::now().time_since_epoch().count() + rand());

			m_taskListByTimerID[timerId] = TaskDurationPair(task, interval);
		}

		m_workerThread->push(TimeTaskPair(std::chrono::system_clock::now(),
										  std::bind(&Timer::repeatTask, this, timerId)
										 )
							);

		return timerId;
	}

	
	void unInstall(size_t timerId)
	{
		std::unique_lock<stdMutex> lock(m_mutex);
		m_taskListByTimerID.erase(timerId);
	}

private:
	void repeatTask(size_t timerId)
	{
		std::unique_lock<stdMutex> lock(m_mutex);
		auto it = m_taskListByTimerID.find(timerId);
		if (it != m_taskListByTimerID.end())
		{
			TaskDurationPair taskSchedulingInfo = it->second;

			//Caution! a possible race condition is that just after unlock, the
			//client code uninstalls the timer and the predicate is a member function
			//if just after the uninstall, the object is deleted before the function completes execution, it can lead to a crash
			//Alternate way to handle it would be to execute the predicate inside lock but then it 
			//can will block the install function and the execution of other installed timers until
			//the predicate finishes execution, this is more serious considering this is an api for scheduling tasks
			//and this can make other timers miss their schedule
			lock.unlock();
			taskSchedulingInfo.first();
			m_workerThread->push(TimeTaskPair(std::chrono::system_clock::now() + taskSchedulingInfo.second,
											  std::bind(&Timer::repeatTask, this, timerId)
											 )
								);
		}
	}
};

class TimerFactory : public ITimerFactory
{
public:
	ITimer_UPtr create(std::string type)
	{
		return ITimer_UPtr(new Timer(std::make_unique<TimedTaskWorkerThread>(std::make_shared<std::vector<TimeTaskPair>>(), std::make_shared<stdMutex>(), std::make_shared<ConditionVariable>())));
	}
};
