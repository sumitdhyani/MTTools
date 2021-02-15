#include "stdafx.h"
#include <CPPUtilities/WorkerThread.hpp>
#include <CPPUtilities/Timer.hpp>
#include <CPPUtilities/ThreadPool.hpp>

namespace mt = patsUtils::ULMTTools;
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

TEST_F(WorkerThreadTests, TestPauseResume)
{
	mt::WorkerThread worker;
	mtInternal::ConditionVariable cond;

	auto func = [this, &cond]()
	{
		std::this_thread::sleep_for(sleepInterval);
		taskExecutionCounter++;
		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	for (int i = 1; i <= totalTasks/2; i++)
		worker.push(func);
	std::this_thread::sleep_for(sleepInterval*60*(totalTasks/100));
	ASSERT_EQ(taskExecutionCounter.load(), totalTasks/2);//We have waited long enough tot let 50% of the tasks finish

	worker.pause();
	for (int i = 1; i <= totalTasks/2; i++)
		worker.push(func);
	std::this_thread::sleep_for(sleepInterval*60*(totalTasks/100));
	//Even though we have waited long enough to let the tasks finish, but since the worker was paused, there should be no progress
	ASSERT_EQ(taskExecutionCounter.load(), totalTasks/2);
	EXPECT_THROW(worker.pause(), std::runtime_error);

	EXPECT_NO_THROW(worker.resume());
	std::this_thread::sleep_for(sleepInterval*60*(totalTasks/100));
	EXPECT_THROW(worker.resume(), std::runtime_error);

	cond.wait();
	//Wait should end only when all tasks have executed
	ASSERT_EQ(totalTasks, taskExecutionCounter.load());
}


TEST_F(WorkerThreadTests, TestPauseResumeByMultipleThreadsSimultaneouesly)
{
	mt::WorkerThread worker;
	std::thread threads[4];
	int numThreads = sizeof(threads) / sizeof(threads[0]);

	std::atomic<int> exceptionCount = 0;
	for (int i = 0; i < numThreads; i++)
		threads[i] = std::thread([&worker, &exceptionCount]() {try { worker.pause(); } catch (std::runtime_error) { exceptionCount++; } });

	for (int i = 0; i < sizeof(threads) / sizeof(threads[0]); i++)
		threads[i].join();

	//All except 1 thread should have got an exception trying to pause an already paused worker 
	ASSERT_EQ(exceptionCount.load(), numThreads - 1);
	//By the point we reach here, the worker should be paused
	EXPECT_NO_THROW(worker.resume());
	EXPECT_NO_THROW(worker.pause());

	exceptionCount = 0;
	for (int i = 0; i < numThreads; i++)
		threads[i] = std::thread([&worker, &exceptionCount]() {try { worker.resume(); } catch (std::runtime_error) { exceptionCount++; } });

	for (int i = 0; i < sizeof(threads) / sizeof(threads[0]); i++)
		threads[i].join();

	//All except 1 thread should have got an exception trying to resume an already running worker 
	ASSERT_EQ(exceptionCount.load(), numThreads - 1);

	//By the point we reach here, the worker should be resumed 
	EXPECT_NO_THROW(worker.pause());
	EXPECT_NO_THROW(worker.resume());

	//Here the worker should be resumed, hence ready to execute tasks, we will assertthis from the following code
	mtInternal::ConditionVariable cond;
	auto func = [this, &cond]()
	{
		//Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		rand();
		taskExecutionCounter++;
		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	for (int j = 0; j < numThreads; j++)
	{
		threads[j] = std::thread([this, &func, &worker, numThreads]() {
			for (int i = 1; i <= totalTasks / numThreads; i++)
				worker.push(func);
		});
	}

	for (int i = 0; i < numThreads; i++)
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


