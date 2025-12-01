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
    std::map<time_point, std::vector<T>> m_processingQueue;
    std::atomic<bool> m_terminate;
    std::function<void(const T &)> m_processor;
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
          m_thread(stdThread([this]()
                             { run(); }))
    {
    }

    void push(const time_point &t, const T &item)
    {
      {
        stdUniqueLock lock(m_mutex);
        m_itemQueue.emplace_back(t, item);
      }

      m_semaphore.release();
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

          for (auto const &item : local)
            m_processingQueue[item.first].push_back(item.second);
        }

        for (auto it = m_processingQueue.begin();
             it != m_processingQueue.end() && it->first <= ULCommonUtils::now();
             m_processingQueue.erase(it), it = m_processingQueue.begin())
        {
          for (auto const &item : it->second)
          {
            m_processor(item);
          }
        }

        if (auto it = m_processingQueue.begin(); it == m_processingQueue.end())
        {
          m_semaphore.acquire();
        }
        else
        {
          m_semaphore.try_acquire_until(it->first);
        }
      }
    }

    ~Scheduler()
    {
      kill();
    }
  };
}
