#pragma once
#include "WorkerThread.hpp"
#include <unordered_map>

namespace ULMTTools
{
	
	class Timer
	{
		typedef std::pair<Task, duration> TaskDurationPair;

		TaskScheduler_SPtr m_workerThread;
		std::unordered_map<size_t, TaskDurationPair> m_taskListByTimerID;
		stdMutex m_mutex;

		//Incremented everytime a new timer is installed, a simple solution to generating new unique ids
		size_t m_incrementalTimerId;
	public:
		explicit Timer(TaskScheduler_SPtr workerThread):
			m_workerThread(workerThread),
			m_incrementalTimerId(0)
		{
		}

		size_t install(Task task, duration interval)
		{
			size_t timerId = 0;

			{
				std::unique_lock<stdMutex> lock(m_mutex);
				timerId = m_incrementalTimerId++;
				m_taskListByTimerID[timerId] = TaskDurationPair(task, interval);
			}

			m_workerThread->push(std::chrono::system_clock::now(), [this, timerId]() {repeatTask(timerId); });

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
				m_workerThread->push(std::chrono::system_clock::now() + taskSchedulingInfo.second, [this, timerId]() {repeatTask(timerId); });
			}
		}
	};
}