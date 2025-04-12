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


namespace ULMTTools {
class WorkerThread;
class TaskScheduler;
}


namespace mtInternalUtils
{
	//Keep the template parameter as something assignable and copyable, otherwise results may be underministic
	template <class T>
	class FifoConsumerThread
	{
		friend class ULMTTools::WorkerThread;
	protected:
		typedef std::vector<T> ConsumerQueue;
		DEFINE_PTR(ConsumerQueue)

		
	private:
		ConsumerQueue m_queue;
		stdMutex m_mutex;
		ConditionVariable m_cond;
		std::atomic<bool> m_terminate;
		bool m_consumerBusy;//Used to avoid unnecessary signalling of consumer if it is busy processing the queue, purely performance
		bool m_paused;//signifies whether is the thread is paused
		stdThread m_thread;
		std::function<void(T)> m_processor;

		void run()
		{
			while (!m_terminate)
			{
				ConsumerQueue local;

				{
					stdUniqueLock lock(m_mutex);

					if (m_paused)
						m_cond.wait(lock);

					if (m_queue.empty())
					{
						m_consumerBusy = false;
						m_cond.wait(lock);
					}

					m_queue.swap(local);
					m_consumerBusy = true;
				}

				for (auto it = local.begin(); it != local.end(); it++)
					m_processor(*it);
			}

			//If the consumer is killed or destroyed, it should exit only after completing the pending tasks
			//so as not to leave the client code in a state of uncertainty regarding which tasks will be executed and which won't
			//This leaves a clear cut behavior, i.e, all the items pushed before killing or destroying the consumer will be processed
			{
				stdUniqueLock lock(m_mutex);
				if (!m_queue.empty())
				{
					ConsumerQueue local;
					m_queue.swap(local);
					lock.unlock();
					for (auto it = local.begin(); it != local.end(); it++)
						m_processor(*it);
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

	public:
		FifoConsumerThread(std::function<void(T)> predicate)
			:m_processor(predicate)
		{
			m_terminate = false;
			m_consumerBusy = false;
			m_paused = false;
			m_thread = stdThread(&FifoConsumerThread::run, this);
		}

		void push(const T& item)
		{
			{
				stdUniqueLock lock(m_mutex);
				if (m_terminate)
					throw std::runtime_error("The consumer has been killed and is no longer in a state tot process new items");
				m_queue.push_back(item);

				if (!(m_paused || m_consumerBusy))
				{
					lock.unlock();
					m_cond.notify_one();
				}
			}

		}

		void pause()
		{
			stdUniqueLock lock(m_mutex);

			if (m_paused)
			{
				lock.unlock();
				throw std::runtime_error("Thread already paused");
			}
			else
				m_paused = true;
		}

		//returns number of pending items
		size_t size()
		{
			stdUniqueLock lock(m_mutex);
			return m_queue.size();
		}

		void resume()
		{
			stdUniqueLock lock(m_mutex);

			if (!m_paused)
			{
				lock.unlock();
				throw std::runtime_error("Thread already running");
			}
			else
			{
				m_paused = false;
				lock.unlock();
				m_cond.notify_one();
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
		DEFINE_PTR(ConsumerQueue)

	private:
		ConsumerQueue m_itemQueue;
		stdMutex m_mutex;
		ConditionVariable m_cond;
		std::map<time_point, std::vector<T>> m_processingQueue;
		std::atomic<bool> m_terminate;
		stdThread m_thread;
		std::function<void(T)> m_processor;

		void kill()
		{
			stdUniqueLock lock(m_mutex);
			if (!m_terminate)
			{
				m_terminate = true;
				lock.unlock();//Ugly but necessary
				m_cond.notify_all();
				m_thread.join();
			}
		}

	public:

		Scheduler(std::function<void(T)> predicate)
			:m_processor(predicate)
		{
			m_terminate = false;
			m_thread = stdThread(&Scheduler::run, this);
		}

		void push(const time_point& t, const T& item)
		{
			{
				stdUniqueLock lock(m_mutex);
				m_itemQueue.push_back(TimeItemPair(t, item));
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

					for (auto currentItem : local)
						m_processingQueue[currentItem.first].push_back(currentItem.second);
				}

				auto it = m_processingQueue.begin();
				if (!m_processingQueue.empty())
				{
					if (it->first <= ULCommonUtils::now())
					{
						auto& processingQueue = it->second;
						for (auto& item : processingQueue)
							m_processor(item);

						m_processingQueue.erase(it);
					}
					else
						m_cond.wait_until(it->first);
				}
				else
					m_cond.wait();
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
		DEFINE_PTR(ConsumerQueue)

	private:
		ConsumerQueue_SPtr m_queue;
		stdMutex m_mutex;
		ConditionVariable m_cond;
		std::atomic<bool> m_terminate;
		bool m_consumerBusy;//Used to avoid unnecessary signalling of consumer if it is busy processing the queue, purely performance
		stdThread m_thread;
		std::function<void(T)> m_processor;
		duration m_unitTime;
		ULCommonUtils::RingBuffer<time_point> m_transactionLog;

		void run()
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
					if (m_transactionLog.full() &&
						((ULCommonUtils::now() - m_transactionLog.front()) <= m_unitTime)
						)
						std::this_thread::sleep_until(m_transactionLog.front() + m_unitTime);

					m_transactionLog.push(ULCommonUtils::now());
					m_processor(currentItem);
				}
			}
		}


	public:

		ThrottledConsumerThread(ConsumerQueue_SPtr queue, std::function<void(T)> predicate, duration unitTime, size_t numTransactions)
			:m_queue(queue),
			m_processor(predicate),
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
				m_queue->push_back(item);

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
		std::shared_ptr<ULMTTools::WorkerThread> m_worker;
		std::shared_ptr<ULMTTools::TaskScheduler> m_scheduler;
		std::queue<T> m_pendingQueue;
		std::function<void(T)> m_processor;
		duration m_unitTime;
		size_t m_numTransactions;
		ULCommonUtils::RingBuffer<time_point> m_transactionLog;

		void processItemAndUpdateTransactionLog(T item)
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
			m_scheduler->push(scheduleTime, [this, scheduleTime]()
			{
				m_worker->push([this, scheduleTime]()
				{
					onBandwidthAvailable(scheduleTime);
				});
			});
		}

		void tryProcess(T item)
		{
			//there are still some pending items so, queue it up behind them, even if the bandwidth is available
			//so as not to spoil the fifo order
			if (!m_pendingQueue.empty())
				m_pendingQueue.push(item);
			else if (!bandWidthAvailable())//no pending items but bandwidth is unavailable, scehdule the processing event for next available timeslot
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

		ReusableThrottler(std::shared_ptr<ULMTTools::WorkerThread> worker,
			std::shared_ptr<ULMTTools::TaskScheduler> scheduler,
			std::function<void(T)> predicate,
			duration unitTime,
			size_t numTransactions
		)
			:m_worker(worker),
			m_scheduler(scheduler),
			m_processor(predicate),
			m_unitTime(unitTime),
			m_numTransactions(numTransactions),
			m_transactionLog(numTransactions)
		{
		}

		void push(const T& item)
		{
			m_worker->push([this, item]() {tryProcess(item); });
		}

		void kill()
		{
		}
	};
}
