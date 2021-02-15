#include <gtest/gtest.h>
#include <TaskThrottlers.hpp>
#include <chrono>
#include <CommonUtils/CommonDefs.hpp>
#include <gtest/gtest.h>

namespace mt = ULMTTools;
namespace mtInternal = mtInternalUtils;

#define BILLION 1000000000

struct ThrottlingTests : ::testing::Test 
{
	int totalTasks;
	std::atomic<int> taskExecutionCounter;
	std::vector<time_point> taskExecutionTimestamps;
	duration unitTime;
	size_t numTasksPerUnitTime;
	virtual void SetUp()
	{
		numTasksPerUnitTime = 1000;
		totalTasks = numTasksPerUnitTime * 10;
		taskExecutionCounter.store(0);
		unitTime = std::chrono::seconds(1);
	}
};

struct ReusableThrottlerTests : ::testing::Test 
{
	size_t numTasksPerUnitTime;
	size_t totalTasks1;
	size_t totalTasks2;
	size_t totalTasks;
	duration unitTime;
	size_t bandwidth1;//1000 transactions/sec
	size_t bandwidth2;//2000 transactions/sec
	std::vector<time_point> taskExecutionTimestamps1;
	std::vector<time_point> taskExecutionTimestamps2;
	size_t taskExecutionCounter;
	virtual void SetUp()
	{
		unitTime = std::chrono::seconds(1);
		bandwidth1 = 1000;//100 transactions/sec
		bandwidth2 = 2000;//200 transactions/sec
		totalTasks1 = bandwidth1 * 10;
		totalTasks2 = bandwidth2 * 10;
		totalTasks = totalTasks1 + totalTasks2;
		taskExecutionCounter = 0;
	}
};

TEST_F(ThrottlingTests, SingleThreaded)
{
	mt::ThrottledWorkerThread throttler(unitTime, numTasksPerUnitTime);
	mtInternal::ConditionVariable cond;

	auto func = [this, &cond]()
	{
		//Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		auto now = ULCommonUtils::now();
		taskExecutionTimestamps.push_back(now);
		//std::cout << "Task " << taskExecutionCounter << ", Time since 1st task = " << (now - taskExecutionTimestamps[0]).count()<<std::endl;
		rand();
		taskExecutionCounter++;

		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	for (int i = 1; i <= totalTasks; i++)
	{
		throttler.push(func);
	}

	cond.wait();
	ASSERT_EQ(taskExecutionCounter, totalTasks);
	ASSERT_EQ(taskExecutionTimestamps.size(), totalTasks);
	
	for (auto [timeWindowStart, timeWindowEnd, startIndex, endIndex] = std::tuple{taskExecutionTimestamps[0],taskExecutionTimestamps[0] + unitTime, 0, 1};
		 endIndex <= taskExecutionTimestamps.size();
		 endIndex++
		)
	{
		if (endIndex == taskExecutionTimestamps.size())
		{
			ASSERT_EQ(endIndex - startIndex, numTasksPerUnitTime);
			ASSERT_LE(taskExecutionTimestamps[endIndex - 1] - taskExecutionTimestamps[startIndex], unitTime);
		}
		else if (taskExecutionTimestamps[endIndex] >= timeWindowEnd)
		{
			ASSERT_EQ(endIndex - startIndex, numTasksPerUnitTime);
			ASSERT_LE(taskExecutionTimestamps[endIndex - 1] - taskExecutionTimestamps[startIndex], unitTime);
			timeWindowStart += unitTime;
			timeWindowEnd = timeWindowStart + unitTime;
			startIndex = endIndex;
		}
	}
}

TEST_F(ThrottlingTests, TestPushingTasksFromMultipleThreads)
{
	mt::ThrottledWorkerThread throttler(unitTime, numTasksPerUnitTime);
	mtInternal::ConditionVariable cond;

	auto func = [this, &cond]()
	{
		//Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		auto now = ULCommonUtils::now();
		taskExecutionTimestamps.push_back(now);
		//std::cout << "Task " << taskExecutionCounter << ", Time since 1st task = " << (now - taskExecutionTimestamps[0]).count()<<std::endl;
		rand();
		taskExecutionCounter++;

		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	std::thread threads[4];
	int numThreads = sizeof(threads) / sizeof(threads[0]);

	auto funcThread = [this, &throttler, &func, &numThreads]()
	{
		for (int i = 1; i <= totalTasks/numThreads; i++)
			throttler.push(func);
	};

	for (int i = 0; i < numThreads; i++)
		threads[i] = std::thread(funcThread);

	for (int i = 0; i < numThreads; i++)
		threads[i].join();

	cond.wait();
	ASSERT_EQ(taskExecutionCounter, totalTasks);
	ASSERT_EQ(taskExecutionTimestamps.size(), totalTasks);
	
	for (auto [timeWindowStart, timeWindowEnd, startIndex, endIndex] = std::tuple{taskExecutionTimestamps[0],taskExecutionTimestamps[0] + unitTime, 0, 1};
		 endIndex <= taskExecutionTimestamps.size();
		 endIndex++
		)
	{
		if (endIndex == taskExecutionTimestamps.size())
		{
			ASSERT_EQ(endIndex - startIndex, numTasksPerUnitTime);
			ASSERT_LE(taskExecutionTimestamps[endIndex - 1] - taskExecutionTimestamps[startIndex], unitTime);
		}
		else if (taskExecutionTimestamps[endIndex] >= timeWindowEnd)
		{
			ASSERT_EQ(endIndex - startIndex, numTasksPerUnitTime);
			ASSERT_LE(taskExecutionTimestamps[endIndex - 1] - taskExecutionTimestamps[startIndex], unitTime);
			timeWindowStart += unitTime;
			timeWindowEnd = timeWindowStart + unitTime;
			startIndex = endIndex;
		}
	}
}

TEST_F(ReusableThrottlerTests, SingleThreaded)
{
	auto worker = std::make_shared<mt::WorkerThread>();
	auto scheduler = std::make_shared<mt::TaskScheduler>();
	
	mt::ReusableThrottledWorkerThread throttler1(worker, scheduler, unitTime, bandwidth1);
	mt::ReusableThrottledWorkerThread throttler2(worker, scheduler, unitTime, bandwidth2);
	mtInternal::ConditionVariable cond;


	auto func1 = [this, &cond]()
	{
		auto now = ULCommonUtils::now();
		taskExecutionTimestamps1.push_back(now);
		//Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		rand();
		taskExecutionCounter++;

		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	auto func2 = [this, &cond]()
	{
		auto now = ULCommonUtils::now();
		taskExecutionTimestamps2.push_back(now);
		//Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		rand();
		taskExecutionCounter++;

		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	for (int i = 1; i <= totalTasks1; i++)
	{
		throttler1.push(func1);
	}

	for (int i = 1; i <= totalTasks2; i++)
	{
		throttler2.push(func2);
	}

	cond.wait();
	ASSERT_EQ(taskExecutionCounter, totalTasks);
	ASSERT_EQ(taskExecutionTimestamps1.size(), totalTasks1);
	ASSERT_EQ(taskExecutionTimestamps2.size(), totalTasks2);
	
	for (auto [timeWindowStart, timeWindowEnd, startIndex, endIndex] = std::tuple{taskExecutionTimestamps1[0],taskExecutionTimestamps1[0] + unitTime, 0, 1};
		 endIndex <= taskExecutionTimestamps1.size();
		 endIndex++
		)
	{
		if (endIndex == taskExecutionTimestamps1.size())
		{
			ASSERT_EQ(endIndex - startIndex, bandwidth1);
			ASSERT_LE(taskExecutionTimestamps1[endIndex - 1] - taskExecutionTimestamps1[startIndex], unitTime);
		}
		else if (taskExecutionTimestamps1[endIndex] >= timeWindowEnd)
		{
			ASSERT_EQ(endIndex - startIndex, bandwidth1);
			ASSERT_LE(taskExecutionTimestamps1[endIndex - 1] - taskExecutionTimestamps1[startIndex], unitTime);
			timeWindowStart += unitTime;
			timeWindowEnd = timeWindowStart + unitTime;
			startIndex = endIndex;
		}
	}

	for (auto [timeWindowStart, timeWindowEnd, startIndex, endIndex] = std::tuple{taskExecutionTimestamps2[0],taskExecutionTimestamps2[0] + unitTime, 0, 1};
		 endIndex <= taskExecutionTimestamps2.size();
		 endIndex++
		)
	{
		if (endIndex == taskExecutionTimestamps2.size())
		{
			ASSERT_EQ(endIndex - startIndex, bandwidth2);
			ASSERT_LE(taskExecutionTimestamps2[endIndex - 1] - taskExecutionTimestamps2[startIndex], unitTime);
		}
		else if (taskExecutionTimestamps2[endIndex] >= timeWindowEnd)
		{
			//Ensure we are not overdoing the bandwidth
			ASSERT_EQ(endIndex - startIndex, bandwidth2);
			//Ensure we are not underdoing the bandwidth
			ASSERT_LE(taskExecutionTimestamps2[endIndex - 1] - taskExecutionTimestamps2[startIndex], unitTime);
			timeWindowStart += unitTime;
			timeWindowEnd = timeWindowStart + unitTime;
			startIndex = endIndex;
		}
	}
}

TEST_F(ReusableThrottlerTests, TestPushingTasksFromMultipleThreads)
{
	auto worker = std::make_shared<mt::WorkerThread>();
	auto scheduler = std::make_shared<mt::TaskScheduler>();
	mt::ReusableThrottledWorkerThread throttler1(worker, scheduler, unitTime, bandwidth1);
	mt::ReusableThrottledWorkerThread throttler2(worker, scheduler, unitTime, bandwidth2);
	mtInternal::ConditionVariable cond;

	auto func1 = [this, &cond]()
	{
		auto now = ULCommonUtils::now();
		taskExecutionTimestamps1.push_back(now);
		//Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		rand();
		taskExecutionCounter++;

		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	auto func2 = [this, &cond]()
	{
		auto now = ULCommonUtils::now();
		taskExecutionTimestamps2.push_back(now);
		//Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		rand();
		taskExecutionCounter++;

		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	auto funcThread1 = [this, &func1, &throttler1]() 
	{
		for (int i = 1; i <= totalTasks1; i++)
		{
			throttler1.push(func1);
		}
	};

	auto funcThread2 = [this, &func2, &throttler2]()
	{
		for (int i = 1; i <= totalTasks2; i++)
		{
			throttler2.push(func2);
		}
	};

	std::thread funcThreads[2] = { std::thread(funcThread1), std::thread(funcThread2) };
	auto numThreads = sizeof(funcThreads) / sizeof(funcThreads[0]);
	for (int i = 0; i < numThreads; i++)
		funcThreads[i].join();

	cond.wait();
	ASSERT_EQ(taskExecutionCounter, totalTasks);
	ASSERT_EQ(taskExecutionTimestamps1.size(), totalTasks1);
	ASSERT_EQ(taskExecutionTimestamps2.size(), totalTasks2);
	
	for (auto [timeWindowStart, timeWindowEnd, startIndex, endIndex] = std::tuple{taskExecutionTimestamps1[0],taskExecutionTimestamps1[0] + unitTime, 0, 1};
		 endIndex <= taskExecutionTimestamps1.size();
		 endIndex++
		)
	{
		if (endIndex == taskExecutionTimestamps1.size())
		{
			ASSERT_EQ(endIndex - startIndex, bandwidth1);
			ASSERT_LE(taskExecutionTimestamps1[endIndex - 1] - taskExecutionTimestamps1[startIndex], unitTime);
		}
		else if (taskExecutionTimestamps1[endIndex] >= timeWindowEnd)
		{
			ASSERT_EQ(endIndex - startIndex, bandwidth1);
			ASSERT_LE(taskExecutionTimestamps1[endIndex - 1] - taskExecutionTimestamps1[startIndex], unitTime);
			timeWindowStart += unitTime;
			timeWindowEnd = timeWindowStart + unitTime;
			startIndex = endIndex;
		}
	}

	for (auto [timeWindowStart, timeWindowEnd, startIndex, endIndex] = std::tuple{taskExecutionTimestamps2[0],taskExecutionTimestamps2[0] + unitTime, 0, 1};
		 endIndex <= taskExecutionTimestamps2.size();
		 endIndex++
		)
	{
		if (endIndex == taskExecutionTimestamps2.size())
		{
			ASSERT_EQ(endIndex - startIndex, bandwidth2);
			ASSERT_LE(taskExecutionTimestamps2[endIndex - 1] - taskExecutionTimestamps2[startIndex], unitTime);
		}
		else if (taskExecutionTimestamps2[endIndex] >= timeWindowEnd)
		{
			//Ensure we are not overdoing the bandwidth
			ASSERT_EQ(endIndex - startIndex, bandwidth2);
			//Ensure we are not underdoing the bandwidth
			ASSERT_LE(taskExecutionTimestamps2[endIndex - 1] - taskExecutionTimestamps2[startIndex], unitTime);
			timeWindowStart += unitTime;
			timeWindowEnd = timeWindowStart + unitTime;
			startIndex = endIndex;
		}
	}
}

