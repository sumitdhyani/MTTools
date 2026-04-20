#pragma once
#include <functional>
#include <mutex>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <thread>
#include <memory>
#include <concepts>
#include "CommonUtils/CommonDefs.hpp"
namespace mtInternalUtils
{
	template <class T>
	requires std::is_copy_constructible_v<T>
	class FifoConsumerThread final
	{
	protected:
		typedef std::vector<T> ConsumerQueue;

		
	private:
		ConsumerQueue m_queue;
		stdMutex m_mutex;
		stdConditionVariable m_cond;
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
					ConsumerQueue local(std::move(m_queue));
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

		bool push(T&& item)
		{
			{
				stdUniqueLock lock(m_mutex);
				if (m_terminate) return false;

				m_queue.push_back(std::move(item));

				if (m_consumerWaiting)
				{
					lock.unlock();
					m_cond.notify_one();
				}
			}

			return true;
		}

		template <class... Args>
		bool emplace(const Args&... args)
		{
			{
				stdUniqueLock lock(m_mutex);
				if (m_terminate)
					return false;

				m_queue.emplace_back(args...);

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
}
