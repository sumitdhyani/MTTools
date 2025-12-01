#pragma once
#include <functional>
#include <queue>
#include <map>
#include <mutex>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <thread>
#include <memory>
#include <concepts>
#include "CommonUtils/RingBuffer.hpp"
#include "CommonUtils/CommonDefs.hpp"


namespace mtInternalUtils 
{
  template <class T>
    requires std::is_copy_constructible_v<T>
  class ThrottledConsumerThread final
  {
  protected:
    typedef std::vector<T> ConsumerQueue;

  private:
    ConsumerQueue m_queue;
    stdMutex m_mutex;
    stdConditionVariable m_cond;
    std::atomic<bool> m_terminate;
    bool m_consumerBusy; // Used to avoid unnecessary signalling of consumer if it is busy processing the queue, purely performance
    stdThread m_thread;
    std::function<void(const T &)> m_processor;
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

        for (auto const &item : local)
        {
          if (m_transactionLog.full() &&
              (ULCommonUtils::now() - m_transactionLog.front() <= m_unitTime))
          {
            std::this_thread::sleep_until(m_transactionLog.front() + m_unitTime);
          }

          m_transactionLog.push(ULCommonUtils::now());
          m_processor(item);
        }
      }
    }

  public:
    ThrottledConsumerThread(std::function<void(const T &)> processor, duration unitTime, size_t numTransactions)
        : m_processor(processor),
          m_unitTime(unitTime),
          m_transactionLog(numTransactions)
    {
      m_terminate = false;
      m_consumerBusy = false;
      m_thread = stdThread(&ThrottledConsumerThread::run, this);
    }

    void push(const T &item)
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

    template <class... Args>
    void emplace(const Args &...args)
    {
      {
        stdUniqueLock lock(m_mutex);
        m_queue.emplace_back(args...);

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

  template <typename T>
  concept WorkerThreadInterface =
      requires(T t, Task task) {
        { t.push(task) } -> std::same_as<bool>;
      };

  template <class T>
  concept SchedulerInterface =
      requires(T t, time_point tp, Task task) {
        { t.push(tp, task) } -> std::same_as<void>;
      };

  template <class T, class Worker, class Sched>
    requires std::is_copy_constructible_v<T> &&
            WorkerThreadInterface<Worker> &&
            SchedulerInterface<Sched>
  class ReusableThrottler final
  {
  private:
    std::shared_ptr<Worker> m_worker;
    std::shared_ptr<Sched> m_scheduler;
    std::queue<T> m_pendingQueue;
    std::function<void(const T &)> m_processor;
    duration m_unitTime;
    size_t m_numTransactions;
    ULCommonUtils::RingBuffer<time_point> m_transactionLog;

    void processItemAndUpdateTransactionLog(const T &item)
    {
      m_processor(item);
      m_transactionLog.push(ULCommonUtils::now());
    }

    bool bandWidthAvailable()
    {
      return !m_transactionLog.full() ||
            (ULCommonUtils::now() - m_transactionLog.front()) >= m_unitTime;
    }

    void scheduleBandwidthAvailableEvent(time_point scheduleTime)
    {
      m_scheduler->push(scheduleTime, [this, scheduleTime]()
      { m_worker->push([this, scheduleTime]()
        { 
          onBandwidthAvailable(scheduleTime); 
        }); 
      });
    }

    void tryProcess(const T &item)
    {
      // there are still some pending items so, queue it up behind them, even if the bandwidth is available
      // so as not to spoil the fifo order
      [[likely]] if (!m_pendingQueue.empty())
      {
        m_pendingQueue.push(item);
      }
      else [[unlikely]] if (!bandWidthAvailable()) // no pending items but bandwidth is unavailable, scehdule the processing event for next available timeslot
      {
        m_pendingQueue.push(item);
        scheduleBandwidthAvailableEvent(m_transactionLog.front() + m_unitTime);
      }
      else // No pending items and bandwidth is available, so process it right away
      {
        processItemAndUpdateTransactionLog(item);
      }
    }

    //"allotedTime" parameter will be useful for debugging purposes to see
    // how much delay is there between alloted time and the actual invocation of the method
    void onBandwidthAvailable(time_point allotedTime)
    {
      while (bandWidthAvailable() && !m_pendingQueue.empty())
      {
        processItemAndUpdateTransactionLog(m_pendingQueue.front());
        m_pendingQueue.pop();
      }

      // pending queue was not processed because of insufficient bandwidth,
      // reschedule the bandwidth available event for the next slot of bandwidth availability
      if (!m_pendingQueue.empty())
      {
        scheduleBandwidthAvailableEvent(m_transactionLog.front() + m_unitTime);
      }
    }

  public:
    ReusableThrottler(const std::shared_ptr<Worker> &worker,
                      const std::shared_ptr<Sched> &scheduler,
                      std::function<void(const T &)> processor,
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

    bool push(const T &item)
    {
      return m_worker->push([this, item](){ tryProcess(item); });
    }
  };
}