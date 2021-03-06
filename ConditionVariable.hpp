#pragma once
#include <mutex>
#include <condition_variable>
#include <functional>
#include "CommonUtils/CommonDefs.hpp"

namespace mtInternalUtils
{
	class ConditionVariable
	{
		stdConditionVariable m_cond;
		std::mutex m_mutex;
		bool m_signalled;

	public:
		ConditionVariable()
		{
			m_signalled = false;
		}


		~ConditionVariable()
		{
		}

		//Whenever in Critical section and have the instance of unique lock, use the versions of wait() with 'lock'
		//as waiting in a critical section without releasing the lock will be the last thing we want to do

		void wait()
		{
			stdUniqueLock lock(m_mutex);
			if (!m_signalled)
				m_cond.wait(lock);
			m_signalled = false;
		}

		void wait(stdUniqueLock& applicationLock)
		{
			applicationLock.unlock();
			wait();
			applicationLock.lock();
		}

		void wait_until(time_point time)
		{
			stdUniqueLock lock(m_mutex);
			if (!m_signalled)
				m_cond.wait_until(lock, time);
			m_signalled = false;
		}

		void wait_until(time_point time, stdUniqueLock& applicationLock)
		{
			applicationLock.unlock();
			wait_until(time);
			applicationLock.lock();
		}


		void wait_for(duration duration)
		{
			stdUniqueLock lock(m_mutex);
			if (!m_signalled)
				m_cond.wait_for(lock, duration);
			m_signalled = false;
		}

		void wait_for(duration duration, stdUniqueLock& applicationLock)
		{
			applicationLock.unlock();
			wait_for(duration);
			applicationLock.lock();
		}

		void notify_one()
		{
			{
				stdUniqueLock lock(m_mutex);
				m_signalled = true;
			}
			m_cond.notify_one();
		}

		void notify_all()
		{
			{
				stdUniqueLock lock(m_mutex);
				m_signalled = true;
			}
			m_cond.notify_all();
		}
	};
	DEFINE_PTR(ConditionVariable)
}