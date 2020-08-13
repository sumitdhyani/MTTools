#pragma once
#include <functional>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <memory>
#include <CommonUtils/RingBuffer.h>
#include "ConditionVariable.h"

namespace utils = ULCommonUtils;

namespace ULMTTools
{

	template <class T>
	class IConsumerThread
	{
	public:
		virtual void kill() = 0;
		virtual void push(T item) = 0;
		virtual ~IConsumerThread() {}
	};


	template <class T>
	class ISuspendableConsumerThread : public IConsumerThread<T>
	{
	public:
		virtual void pause() = 0;
		virtual void resume() = 0;
		virtual ~ISuspendableConsumerThread() {}
	};

	class WorkerThread;
	//Keep the template parameter as something assignable and copyable, otherwise results may be underministic
	template <class T>
	class FifoConsumerThread : public ISuspendableConsumerThread<T>
	{
		friend class WorkerThread;
	protected:
		typedef std::vector<T> ConsumerQueue;
		DEFINE_PTR(ConsumerQueue)

			//Visible to only subclasses, as owning this condition variable by the client code can be dangerous
			//in case the application programmer tries to do something fancy but is not aware of the dangerous edge cases
			//The most likely side effects can then be an infinitely waiting thread or unexpected spurious wakeups
			FifoConsumerThread(ConsumerQueue_SPtr queue, stdMutex_SPtr mutex, std::function<void(T)> predicate, ConditionVariable_SPtr cond)
			:m_queue(queue),
			m_mutex(mutex),
			m_predicate(predicate),
			m_cond(cond)
		{
			m_terminate = false;
			m_consumerBusy = false;
			m_paused = false;
			m_thread = stdThread(&FifoConsumerThread::run, this);
		}
	private:
		ConsumerQueue_SPtr m_queue;
		stdMutex_SPtr m_mutex;
		ConditionVariable_SPtr m_cond;
		std::atomic<bool> m_terminate;
		bool m_consumerBusy;//Used to avoid unnecessary signalling of consumer if it is busy processing the queue, purely performance
		bool m_paused;//signifies whether is the thread is paused
		stdThread m_thread;
		std::function<void(T)> m_predicate;

		virtual void run()
		{
			while (!m_terminate)
			{
				ConsumerQueue local;

				{
					stdUniqueLock lock(*m_mutex);

					if (m_paused)
						m_cond->wait(lock);

					if (m_queue->empty())
					{
						m_consumerBusy = false;
						m_cond->wait(lock);
					}

					m_queue->swap(local);
					m_consumerBusy = true;
				}

				for (auto const& currentItem : local)
					m_predicate(currentItem);
			}
		}


	public:

		FifoConsumerThread(ConsumerQueue_SPtr queue, stdMutex_SPtr mutex, std::function<void(T)> predicate)
			:FifoConsumerThread(queue, mutex, predicate, ConditionVariable_SPtr(new ConditionVariable))
		{
		}


		virtual void push(T item)
		{
			{
				stdUniqueLock lock(*m_mutex);
				m_queue->push_back(item);

				if (!(m_paused || m_consumerBusy))
				{
					lock.unlock();
					m_cond->notify_one();
				}
			}

		}

		virtual void pause()
		{
			stdUniqueLock lock(*m_mutex);

			if (m_paused)
			{
				lock.unlock();
				throw std::runtime_error("Thread already paused");
			}
			else
				m_paused = true;
		}



		virtual void resume()
		{
			stdUniqueLock lock(*m_mutex);

			if (!m_paused)
			{
				lock.unlock();
				throw std::runtime_error("Thread already running");
			}
			else
			{
				m_paused = false;
				lock.unlock();
				m_cond->notify_one();
			}
		}

		virtual void kill()
		{
			stdUniqueLock lock(*m_mutex);
			if (!m_terminate)
			{
				m_terminate = true;
				lock.unlock();
				//To avoid a spurious wakeup during kill process, to ensure we are able to wakeup the
				//Thread owned by this object, we need to notify_all on this condition variable as this condition variable
				//is shared by all the threads in case they belong to a threadpool
				//Downside is that all threads will be woken up and contention
				//is likely to happen between rest of the peer threads in the pool
				//In case this is a standalone thread, i.e. a thread which is not a part of the threadpool, no performance
				//impact will be there
				//This will certainly involve spurious wakeups but the thing is we know when these spuirious wakeups will occur and the 
				//impact and are ok with it
				m_cond->notify_all();
				m_thread.join();
			}
		}

		~FifoConsumerThread()
		{
			kill();
		}
	};

	template <class T>
	class TimedConsumerThread : public IConsumerThread<std::pair<std::chrono::system_clock::time_point, T>>
	{
	protected:
		typedef std::pair<std::chrono::system_clock::time_point, T> TimeItemPair;
		typedef std::vector<TimeItemPair> ConsumerQueue;
		DEFINE_PTR(ConsumerQueue)

	private:
		ConsumerQueue_SPtr m_itemQueue;
		stdMutex_SPtr m_mutex;
		ConditionVariable_SPtr m_cond;
		std::map<std::chrono::system_clock::time_point, std::vector<T>> m_processingQueue;
		std::atomic<bool> m_terminate;
		stdThread m_thread;
		std::function<void(T)> m_predicate;

	public:

		TimedConsumerThread(ConsumerQueue_SPtr queue, stdMutex_SPtr mutex, std::function<void(T)> predicate, ConditionVariable_SPtr cond)
			:m_itemQueue(queue),
			m_mutex(mutex),
			m_predicate(predicate),
			m_cond(cond)
		{
			m_terminate = false;
			m_thread = stdThread(&TimedConsumerThread::run, this);
		}


		TimedConsumerThread(ConsumerQueue_SPtr queue, stdMutex_SPtr mutex, std::function<void(T)> predicate)
			:TimedConsumerThread(queue, mutex, predicate, ConditionVariable_SPtr(new ConditionVariable))
		{
		}

		virtual void push(TimeItemPair timeItemPair)
		{
			{
				stdUniqueLock lock(*m_mutex);
				m_itemQueue->push_back(timeItemPair);
			}

			m_cond->notify_one();
		}

		virtual void run()
		{
			while (!m_terminate)
			{
				{
					ConsumerQueue local;

					{
						stdUniqueLock lock(*m_mutex);
						m_itemQueue->swap(local);
					}

					for (auto currentItem : local)
						m_processingQueue[currentItem.first].push_back(currentItem.second);
				}

				auto it = m_processingQueue.begin();
				if (!m_processingQueue.empty())
				{
					if (it->first <= std::chrono::system_clock::now())
					{
						auto& processingQueue = it->second;
						for (auto& item : processingQueue)
							m_predicate(item);

						m_processingQueue.erase(it);
					}
					else
						m_cond->wait_until(it->first);
				}
				else
					m_cond->wait();
			}
		}

		virtual void kill()
		{
			stdUniqueLock lock(*m_mutex);
			if (!m_terminate)
			{
				m_terminate = true;
				lock.unlock();//Ugly but necessary
				m_cond->notify_all();
				m_thread.join();
			}
		}

		~TimedConsumerThread()
		{
			kill();
		}
	};


	template <class T>
	class ThrottledConsumerThread : public IConsumerThread<T>
	{
	protected:
		typedef std::vector<T> ConsumerQueue;
		DEFINE_PTR(ConsumerQueue)

	private:
		ConsumerQueue_SPtr m_queue;
		stdMutex m_mutex;
		ConditionVariable m_cond;
		std::atomic<bool> m_terminate;
		bool m_consumerBusy;//Used to avoid unnecessary signalling of consumer if it is busy processing the queue, purely performance
		stdThread m_thread;
		std::function<void(T)> m_predicate;
		duration m_unitTime;
		size_t m_numTransactions;
		utils::RingBuffer<time_point> m_transactionLog;

		virtual void run()
		{
			while (!m_terminate)
			{
				ConsumerQueue local;

				{
					stdUniqueLock lock(m_mutex);

					if (m_queue->empty())
					{
						m_consumerBusy = false;
						m_cond.wait(lock);
					}

					m_queue->swap(local);
					m_consumerBusy = true;
				}

				for (auto const& currentItem : local)
				{
					if ((m_transactionLog.size() == m_numTransactions) &&
						((utils::now() - m_transactionLog.front()) < m_unitTime)
						)
						m_cond.wait_until(m_transactionLog.front() + m_unitTime);

					m_transactionLog.push(utils::now());
					m_predicate(currentItem);
				}
			}
		}


	public:

		ThrottledConsumerThread(ConsumerQueue_SPtr queue, std::function<void(T)> predicate, duration unitTime, size_t numTransactions)
			:m_queue(queue),
			m_predicate(predicate),
			m_unitTime(unitTime),
			m_numTransactions(numTransactions),
			m_transactionLog(numTransactions)
		{
			m_terminate = false;
			m_consumerBusy = false;
			m_thread = stdThread(&ThrottledConsumerThread::run, this);
		}

		virtual void push(T item)
		{
			{
				stdUniqueLock lock(m_mutex);
				m_queue->push_back(item);

				if (!m_consumerBusy)
				{
					lock.unlock();
					m_cond.notify_one();
				}
			}
		}

		virtual void kill()
		{
			stdUniqueLock lock(m_mutex);
			if (!m_terminate)
			{
				m_terminate = true;
				lock.unlock();
				m_cond.notify_one();
				m_thread.join();
			}
		}

		~ThrottledConsumerThread()
		{
			kill();
		}
	};
}