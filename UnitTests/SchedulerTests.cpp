#include <vector>
#include <CommonUtils/CommonDefs.hpp>
#include "TaskScheduler.hpp"
#include <gtest/gtest.h>

namespace mt = ULMTTools;
namespace utils = ULCommonUtils;

struct BasicSchedulerTests : ::testing::Test
{
  std::vector<time_point> taskExecutionTimestamps;
  stdMutex mutex;

  virtual void setup()
  {
    taskExecutionTimestamps.clear();
  }
};



TEST_F(BasicSchedulerTests, Basic)
{
  mt::TaskScheduler scheduler;
  constexpr uint32_t totalTasks = 1000000;
  std::binary_semaphore allTasksCompleted(0);
  std::atomic<uint32_t> tasksCompleted = 0;

  auto func = [&]()
  {
    ++tasksCompleted;
    if (tasksCompleted == totalTasks)
    {
      allTasksCompleted.release();
    }
  };

  for (uint32_t i = 0; i< totalTasks; ++i)
  {
    scheduler.push(utils::now(), func);
  }

  allTasksCompleted.acquire();
}