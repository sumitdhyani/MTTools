#include <WorkerThread.hpp>
#include <gtest/gtest.h>

namespace mt = ULMTTools;
namespace mtInternal = mtInternalUtils;

struct WorkerThreadTests : ::testing::Test 
{
	int totalTasks;
	std::atomic<int> taskExecutionCounter;
	std::chrono::milliseconds sleepInterval;

	virtual void SetUp()
	{
		sleepInterval = std::chrono::milliseconds(10);
		totalTasks = 100;
		taskExecutionCounter.store(0);
	}
};

//Checking that all pushed tasks have executed 
TEST_F(WorkerThreadTests, TestPushingTasksFromSingleThread)
{
	mt::WorkerThread worker;
	mtInternal::ConditionVariable cond;

	auto func = [this, &cond]()
	{
		//Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		rand();
		taskExecutionCounter++;
		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	for (int i = 1; i <= totalTasks; i++)
		worker.push(func);

	cond.wait();
	//Wait should end only when all tasks have executed
	ASSERT_EQ(totalTasks, taskExecutionCounter.load());
}

TEST_F(WorkerThreadTests, TestPushingTasksFromMultipleThreads)
{
	mt::WorkerThread worker;
	mtInternal::ConditionVariable cond;

	auto func = [this, &cond]()
	{
		//Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		rand();
		taskExecutionCounter++;
		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	std::thread threads[4];

	for (int j = 0; j < 4; j++)
	{
		threads[j] = std::thread([this, &func, &worker]() {
			for (int i = 1; i <= totalTasks / 4; i++)
				worker.push(func);
		});
	}

	for (int i = 0; i < 4; i++)
		threads[i].join();

	cond.wait();

	//Wait should end only when all tasks have executed
	ASSERT_EQ(totalTasks, taskExecutionCounter.load());
}

TEST_F(WorkerThreadTests, DISABLED_TestKillByDestruction)
{
	{
		mt::WorkerThread worker;
		mtInternal::ConditionVariable cond;

		auto func = [this, &cond]()
		{
			std::this_thread::sleep_for(sleepInterval);
			taskExecutionCounter++;
		};

		for (int i = 1; i <= totalTasks; i++)
			worker.push(func);
	}
	//The destructor should have blocked until all the pending items were processed
	ASSERT_EQ(taskExecutionCounter.load(), totalTasks);
}

int main(int argc, const char **argv)
{
	::testing::InitGoogleTest();
	return RUN_ALL_TESTS();
}
