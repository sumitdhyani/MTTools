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
#include "CommonUtils/CommonDefs.hpp"

namespace mtInternalUtils
{
  template <class T>
    requires std::is_copy_constructible_v<T>
  class Scheduler final
  {
  protected:
    typedef std::pair<time_point, T> TimeItemPair;
    typedef std::vector<TimeItemPair> ConsumerQueue;

  private:
    ConsumerQueue m_itemQueue;
    stdMutex m_mutex;
    std::binary_semaphore m_semaphore;

    // The main book having the information about scheduled tasks
    // scheduled time -> a list of items to be processed by the m_processor
    std::map<time_point, std::vector<T>> m_processingQueue;

    std::atomic<bool> m_terminate;

    // The functions that is invoked for an item(of type T) when its scheduled time arrives
    std::function<void(const T &)> m_processor;

    // The thread owned by this object
    // When the scheduled time for an item(of type T) arrives,
    // the m_processor is invoked for it in this thread
    stdThread m_thread;

    void kill()
    {
      if (!m_terminate)
      {
        m_terminate = true;
        m_semaphore.release();
        m_thread.join();
      }
    }

  public:
    Scheduler(const std::function<void(const T &)> &processor)
        : m_processor(processor),
          m_terminate(false),
          m_semaphore(0),
          m_thread(stdThread([this](){ run(); }))
    {
    }

    void push(const time_point &t, const T &item)
    {
      {
        // Critical section for shared queue
        stdUniqueLock lock(m_mutex);
        m_itemQueue.emplace_back(t, item);
      }

      // Signal the run() method that there are items whose processing is to be scheduled
      m_semaphore.release();
    }

    void run()
    {
      while (!m_terminate)
      {
        {
          ConsumerQueue local;

          {
            // Critical section for shared queue
            stdUniqueLock lock(m_mutex);
            m_itemQueue.swap(local);
          }

          for (auto const &item : local)
          {
            m_processingQueue[item.first].push_back(item.second);
          }
        }

        // Process all the items whose scheduled time is <= currentTime 
        auto processPendingItems = [&]()
        {
          for (auto it = m_processingQueue.begin();
              it != m_processingQueue.end() && it->first <= ULCommonUtils::now();
              m_processingQueue.erase(it), it = m_processingQueue.begin())
          {
            for (auto const &item : it->second)
            {
              m_processor(item);
            }
          }
        };

        processPendingItems();

        // No scheduled tasks, just wait for some new task to be scheduled
        // by waiting on the semaphore
        if (auto it = m_processingQueue.begin(); it == m_processingQueue.end())
        {
          m_semaphore.acquire();
        }
        // There are stil some pending scheduled tasks whose scheduled time is somewhere in the future
        // Wait until the the schedule time of the earliest scheduled task
        else if (!m_semaphore.try_acquire_until(it->first))
        {
          // We are here, it means that the semaphore was not released within the expiry period
          // of the earliest scheduled task This means that no new tasks were scheduled during
          // the waiting period, so immediately execute all the pending tasks and then go to
          // the beginning of loop to look for new tasks to be scheduled
          processPendingItems();
        }
      }
    }

    ~Scheduler()
    {
      kill();
    }
  };
}
