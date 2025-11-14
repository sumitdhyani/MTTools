#pragma once
#include <functional>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <memory>
#include <CommonUtils/RingBuffer.hpp>
#include "ConditionVariable.hpp"

typedef std::function<void()> Task;


namespace mtInternalUtils
{
	template <class T>
	requires std::is_copy_constructible_v<T>
	class FifoConsumerThread
	{
	protected:
		typedef std::vector<T> ConsumerQueue;
		DEFINE_PTR(ConsumerQueue)

		
	private:
		ConsumerQueue m_queue;
		stdMutex m_mutex;
		ConditionVariable m_cond;
		std::atomic<bool> m_terminate;
		bool m_consumerWaiting;//Used to avoid unnecessary signalling of consumer if it is busy processing the queue, purely performance
		std::function<void(const T&)> m_processor;
		stdThread m_thread;

		void run()
		{
			while (!m_terminate)
			{
				ConsumerQueue local;

				{
					stdUniqueLock lock(m_mutex);
					if (m_queue.empty())
					{
						m_consumerWaiting = true;
						m_cond.wait(lock);
						m_consumerWaiting = false;
					}

					local = std::move(m_queue);
				}

				for(auto const& item : local)
				{
					m_processor(item);
				}
			}

			//If the consumer is killed or destroyed, it should exit only after completing the pending tasks
			//so as not to leave the client code in a state of uncertainty regarding which tasks will be executed and which won't
			//This leaves a clear cut behavior, i.e, all the items pushed before killing or destroying the consumer will be processed
			{
				stdUniqueLock lock(m_mutex);
				if (!m_queue.empty())
				{
					ConsumerQueue local;
					local = std::move(m_queue);
					lock.unlock();
					for(auto const& item : local)
					{
						m_processor(item);
					}
				}
			}
		}

	public:
		FifoConsumerThread(const std::function<void(const T&)> &processor)
			: m_processor(processor),
				m_terminate(false),
				m_consumerWaiting(false),
				m_thread(stdThread([this](){ run(); }))
		{}

		bool push(const T& item)
		{
			{
				stdUniqueLock lock(m_mutex);
				if (m_terminate) return false;

				m_queue.push_back(item);

				if (m_consumerWaiting)
				{
					lock.unlock();
					m_cond.notify_one();
				}
			}

			return true;
		}

		//returns number of pending items
		size_t size()
		{
			stdUniqueLock lock(m_mutex);
			return m_queue.size();
		}

		void kill()
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

		~FifoConsumerThread()
		{
			kill();
		}
	};

	template <class T>
	class Scheduler
	{
	protected:
		typedef std::pair<time_point, T> TimeItemPair;
		typedef std::vector<TimeItemPair> ConsumerQueue;

	private:
		ConsumerQueue m_itemQueue;
		stdMutex m_mutex;
		ConditionVariable m_cond;
		std::map<time_point, std::vector<T>> m_processingQueue;
		std::atomic<bool> m_terminate;
		std::function<void(const T&)> m_processor;
		stdThread m_thread;

		void kill()
		{
			stdUniqueLock lock(m_mutex);
			if (!m_terminate)
			{
				m_terminate = true;
				lock.unlock();//Ugly but necessary
				m_cond.notify_one();
				m_thread.join();
			}
		}

	public:
		Scheduler(const std::function<void(const T&)> &processor)
				: m_processor(processor),
					m_terminate(false),
					m_thread(stdThread([this](){ run(); }))
		{}

		void push(const time_point& t, const T& item)
		{
			{
				stdUniqueLock lock(m_mutex);
				m_itemQueue.emplace_back(t, item);
			}

			m_cond.notify_one();
		}

		void run()
		{
			while (!m_terminate)
			{
				{
					ConsumerQueue local;

					{
						stdUniqueLock lock(m_mutex);
						m_itemQueue.swap(local);
					}

					for (auto const& item : local)
						m_processingQueue[item.first].push_back(item.second);
				}

				for (auto it = m_processingQueue.begin();
						 it != m_processingQueue.end() && it->first <= ULCommonUtils::now();
						 m_processingQueue.erase(it), it = m_processingQueue.begin())
				{
					for (auto const& item : it->second)
					{
						m_processor(item);
					}
				}

				if(auto it = m_processingQueue.begin(); it == m_processingQueue.end())
				{
					m_cond.wait();
				}
				else
				{
					m_cond.wait_until(it->first);
				}
			}
		}

		~Scheduler()
		{
			kill();
		}
	};


	template <class T>
	class ThrottledConsumerThread
	{
	protected:
		typedef std::vector<T> ConsumerQueue;

	private:
		ConsumerQueue m_queue;
		stdMutex m_mutex;
		ConditionVariable m_cond;
		std::atomic<bool> m_terminate;
		bool m_consumerBusy;//Used to avoid unnecessary signalling of consumer if it is busy processing the queue, purely performance
		stdThread m_thread;
		std::function<void(const T&)> m_processor;
		duration m_unitTime;
		ULCommonUtils::RingBuffer<time_point> m_transactionLog;

		void run()
		{
			while (!m_terminate)
			{
				ConsumerQueue local;

				{
					stdUniqueLock lock(m_mutex);

					if (m_queue.empty())
					{
						m_consumerBusy = false;
						m_cond.wait(lock);
					}

					m_queue.swap(local);
					m_consumerBusy = true;
				}

				for (auto const& item : local)
				{
					if (m_transactionLog.full() &&
						((ULCommonUtils::now() - m_transactionLog.front()) <= m_unitTime)
						)
						std::this_thread::sleep_until(m_transactionLog.front() + m_unitTime);

					m_transactionLog.push(ULCommonUtils::now());
					m_processor(item);
				}
			}
		}


	public:

		ThrottledConsumerThread(std::function<void(const T&)> processor, duration unitTime, size_t numTransactions)
			:m_processor(processor),
			m_unitTime(unitTime),
			m_transactionLog(numTransactions)
		{
			m_terminate = false;
			m_consumerBusy = false;
			m_thread = stdThread(&ThrottledConsumerThread::run, this);
		}

		void push(const T& item)
		{
			{
				stdUniqueLock lock(m_mutex);
				m_queue.push_back(item);

				if (!m_consumerBusy)
				{
					lock.unlock();
					m_cond.notify_one();
				}
			}
		}

		void kill()
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


	template <class T>
	class ReusableThrottler
	{
	private:

		// A function that executes the provided task in its own thread, separate from this thread
		typedef std::function<void(const Task &)> WorkerThreadFunction;
		// A function that executes the provided task at the given time in its own thread, separate from this thread
		typedef std::function<void(const time_point&, const Task &)> TaskSchedulerFunction;

		WorkerThreadFunction m_worker;
		TaskSchedulerFunction m_scheduler;
		std::queue<T> m_pendingQueue;
		std::function<void(const T&)> m_processor;
		duration m_unitTime;
		size_t m_numTransactions;
		ULCommonUtils::RingBuffer<time_point> m_transactionLog;

		void processItemAndUpdateTransactionLog(const T& item)
		{
			m_processor(item);
			m_transactionLog.push(ULCommonUtils::now());
		}

		bool bandWidthAvailable()
		{
			return	!((m_transactionLog.full()) &&
					  ((ULCommonUtils::now() - m_transactionLog.front()) < m_unitTime)
					 );
		}

		void scheduleBandwidthAvailableEvent(time_point scheduleTime)
		{
			m_scheduler(scheduleTime, [this, scheduleTime]()
			{
				m_worker([this, scheduleTime]()
				{
					onBandwidthAvailable(scheduleTime);
				});
			});
		}

		void tryProcess(T item)
		{
			//there are still some pending items so, queue it up behind them, even if the bandwidth is available
			//so as not to spoil the fifo order
			[[likely]]if (!m_pendingQueue.empty())
				m_pendingQueue.push(item);
			else [[unlikely]]if (!bandWidthAvailable()) // no pending items but bandwidth is unavailable, scehdule the processing event for next available timeslot
			{
				m_pendingQueue.push(item);
				scheduleBandwidthAvailableEvent(m_transactionLog.front() + m_unitTime);
			}
			else//No pending items and bandwidth is available, so process it right away
				processItemAndUpdateTransactionLog(item);
		}

		//"allotedTime" parameter will be useful for debugging purposes to see
		//how much delay is there between alloted time and the actual invocation of the method
		void onBandwidthAvailable(time_point allotedTime)
		{
			while (bandWidthAvailable() && !m_pendingQueue.empty())
			{
				processItemAndUpdateTransactionLog(m_pendingQueue.front());
				m_pendingQueue.pop();
			}

			//pending queue was not processed because of insufficient bandwidth,
			//reschedule the bandwidth available event for the next slot of bandwidth availability
			if (!m_pendingQueue.empty())
				scheduleBandwidthAvailableEvent(m_transactionLog.front() + m_unitTime);
		}

	public:
		ReusableThrottler(const WorkerThreadFunction& worker,
											const TaskSchedulerFunction& scheduler,
											std::function<void(const T&)> processor,
											duration unitTime,
											size_t numTransactions)
				: m_worker(worker),
					m_scheduler(scheduler),
					m_processor(processor),
					m_unitTime(unitTime),
					m_numTransactions(numTransactions),
					m_transactionLog(numTransactions)
		{
		}

		void push(const T& item)
		{
			m_worker([this, item]() {tryProcess(item); });
		}

		void kill()
		{
		}
	};
}
