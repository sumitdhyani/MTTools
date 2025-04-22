#include <ThreadPool.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <gtest/gtest.h>

namespace mt = ULMTTools;
namespace mtInternal = mtInternalUtils;

struct ThreadPoolTests : ::testing::Test 
{
	int totalTasks;
	std::atomic<int> taskExecutionCounter;
	std::chrono::milliseconds sleepInterval;
	size_t numCores;

	virtual void SetUp()
	{
		sleepInterval = std::chrono::milliseconds(10);
		totalTasks = 100;
		taskExecutionCounter.store(0);
		numCores = std::thread::hardware_concurrency();
	}
};

TEST_F(ThreadPoolTests, TestPushingTasksFromSingleThread)
{
	mt::ThreadPool worker(numCores);
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

TEST_F(ThreadPoolTests, TestPushingTasksFromMultipleThreads)
{
	mt::ThreadPool worker(numCores);
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

TEST_F(ThreadPoolTests, PerformanceVsBoost)
{
	uint8_t numThreads = 4;
	mt::ThreadPool threadPool(numThreads);
	mtInternal::ConditionVariable cond;
	std::atomic<size_t> numTasksExecutedTillNow = 0;

	auto func = [&cond, &numTasksExecutedTillNow](size_t totalTasks , size_t numRepetetions, bool notify)
	{
		size_t random;
		for (size_t i = 0; i < numRepetetions; i++)
			random |= rand();
		random |= rand();

		numTasksExecutedTillNow++;

		if ((totalTasks == numTasksExecutedTillNow.load()) && notify)
			cond.notify_one();
	};

	auto performanceRatioMine = [&threadPool, &func, &cond, &numTasksExecutedTillNow](size_t totalTasks, size_t numRepetetionsPerTask)
	{
		auto now = std::chrono::high_resolution_clock::now;
		auto start = now();
		for (size_t i = 1; i <= totalTasks; i++)
			threadPool.push(std::bind(func, totalTasks, numRepetetionsPerTask, true));
		cond.wait();
		auto t_threadPool = now() - start;

		[&numTasksExecutedTillNow,&totalTasks](){ASSERT_EQ(numTasksExecutedTillNow.load(), totalTasks);}();
		numTasksExecutedTillNow = 0;
		start = now();
		for (int i = 1; i <= totalTasks; i++)
			func(totalTasks, numRepetetionsPerTask, false);
		auto t_singleThread = now() - start;

		numTasksExecutedTillNow = 0;
		return (double)t_threadPool.count() / (double)t_singleThread.count();
	};


	size_t numrepetetionsPerTask = 50;
	size_t numTasks = 100000;
	
	auto averager = [](std::function<double()> func, size_t numRepetetions)
	{
		double total = 0;
		for (size_t i = 0; i < numRepetetions; i++)
			total += func();

		return (total / numRepetetions);
	};

	size_t averagingSampleSize = 10;
	auto speedup_mine = averager(std::bind(performanceRatioMine, numTasks, numrepetetionsPerTask), averagingSampleSize);
	std::cout << "Speedup with " << numTasks << " tasks and with each task  = " << numrepetetionsPerTask << " units and concurrency = " << (int)numThreads << " is " << speedup_mine << std::endl;
}

TEST_F(ThreadPoolTests, DISABLED_TestKillByDestruction)
{
	{
		mt::ThreadPool worker(numCores);
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
