#include <gtest/gtest.h>
#include <TaskThrottlers.hpp>
#include <chrono>
#include <memory>
#include <array>
#include <CommonUtils/CommonDefs.hpp>
#include <gtest/gtest.h>

namespace mt = ULMTTools;
namespace mtInternal = mtInternalUtils;

#define BILLION 1000000000

struct ThrottlingTests : ::testing::Test 
{
	size_t totalTasks;
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
{};

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

	constexpr size_t numThrottlers = 4;
	auto oneSec = std::chrono::seconds(1);

	// unitTimes[i] 			= unitTime of throttlers[i],
	// bandWidth[i] 			= bandWidth of throttlers[i]
	// execTimeStamps[i] 	= execLog for throttlers[i]
	// numTasks[i]				= total tasks to be executed by throttlers[i]
	duration unitTimes[numThrottlers] = {oneSec, oneSec, oneSec, oneSec};
	size_t bandWidths[numThrottlers] = {1000, 2000, 30000, 4000};
	std::vector<time_point> execTimeStamps[numThrottlers];
	size_t numTasks[numThrottlers];

	// total tasks to be executed by all the throttlers combined
	size_t totalTasks = 0;
	for (size_t i = 0; i < numThrottlers; ++i)
	{
		// no. of tasks fo each throttler = bandWidth * 10
		// so all the tasks should be executed in unitTime * 10 seconds, i.e 10 sec
		// as the unitTime for each thread is kept to be 1 sec
		numTasks[i] = bandWidths[i] * 10;
		totalTasks += bandWidths[i] * 10;
	}

	std::unique_ptr<mt::ReusableThrottledWorkerThread> throttlers[numThrottlers];

	for (size_t i = 0; i < numThrottlers; ++i)
	{
		throttlers[i] = std::move(std::make_unique<mt::ReusableThrottledWorkerThread>(worker, scheduler, unitTimes[i], bandWidths[i]));
	}

	mtInternal::ConditionVariable cond;

	// Total tasks executed yet
	size_t taskExecutionCounter = 0;

	// Execute the task and add the time of execution to the execLog
	auto addExecTimeStamp = [this, &cond, &taskExecutionCounter, &totalTasks](std::vector<time_point> &execTimestamp)
	{
		auto now = ULCommonUtils::now();
		execTimestamp.push_back(now);
		// Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		rand();
		taskExecutionCounter++;

		// If all the tasks have been executed, signal the cond variable
		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	// push 'numTasks' tasks to the provided throttler
	// and update its execution log
	auto pushTasks = [&addExecTimeStamp](mt::ReusableThrottledWorkerThread &throttler,
																			 const size_t &numTasks,
																			 std::vector<time_point> &execTimeStamps)
	{
		for (int i = 0; i < numTasks; i++)
		{
			throttler.push([&addExecTimeStamp, &execTimeStamps]()
										 { addExecTimeStamp(execTimeStamps); });
		}
	};

	for (size_t i = 0; i < numThrottlers; ++i)
	{
		pushTasks(*throttlers[i], numTasks[i], execTimeStamps[i]);
	}

	cond.wait();
	ASSERT_EQ(taskExecutionCounter, totalTasks);

	for (size_t i = 0; i < numThrottlers; ++i)
	{
		ASSERT_EQ(execTimeStamps[i].size(), numTasks[i]);
	}

	// Validate that each throttler executed exactly the no. of tasks it delegated
	for (size_t i = 0; i < numThrottlers; ++i)
	{
		ASSERT_EQ(execTimeStamps[i].size(), numTasks[i]);
	}

	// Time to validate the execLog of each throttler
	// Following points are validated in this function:

	auto validateTransactionLog =
			[](const std::vector<time_point> &execLog,
				 const duration &unitTime,
				 const size_t &bandWidth,
				 const size_t &totalTasks)
	{
		// no. of time windows of size 'unitTime' encountered while parsing the execLog
		size_t numSections = 0;
		for (auto [timeWindowStart, timeWindowEnd, startIndex, endIndex] = std::tuple{execLog[0], execLog[0] + unitTime, 0, 1};
				 endIndex <= execLog.size();
				 endIndex++)
		{
			// End of execLog encountered
			if (endIndex == execLog.size())
			{
				// Each 'unitTime' time window should have exactly 'bandWidth' entries
				ASSERT_EQ(endIndex - startIndex, bandWidth);

				// The timespan in which 'bandWidth' tasks excuted should be <= 'unitTime'
				// This is to endure that the throttler did not 'over-throttle' the tasks
				ASSERT_LE(execLog[endIndex - 1] - execLog[startIndex], unitTime);
				++numSections;
			}
			// Curr time window >= unitTime
			else if (execLog[endIndex] >= timeWindowEnd)
			{
				ASSERT_EQ(endIndex - startIndex, bandWidth);
				ASSERT_LE(execLog[endIndex - 1] - execLog[startIndex], unitTime);
				timeWindowStart += unitTime;
				timeWindowEnd = timeWindowStart + unitTime;
				startIndex = endIndex;
				++numSections;
			}
		}

		// numSections should be = totalTasks / bandWidth if every thing was handled perfectly
		// For example if the bandWidth was 1000 and the unitTime was 1 sec
		// Then if i pushed 15000 tasks i should have 15000/1000 = 15 sections of 1000 entries
		// in the execLog
		ASSERT_EQ(numSections, (size_t)(totalTasks / bandWidth));
	};

	for (size_t i = 0; i < numThrottlers; ++i)
	{
		validateTransactionLog(execTimeStamps[i], unitTimes[i], bandWidths[i], numTasks[i]);
	}
}

TEST_F(ReusableThrottlerTests, TestPushingTasksFromMultipleThreads)
{
	auto worker = std::make_shared<mt::WorkerThread>();
	auto scheduler = std::make_shared<mt::TaskScheduler>();

	constexpr size_t numThrottlers = 4;
	auto oneSec = std::chrono::seconds(1);

	// unitTimes[i] 			= unitTime of throttlers[i],
	// bandWidth[i] 			= bandWidth of throttlers[i]
	// execTimeStamps[i] 	= execLog for throttlers[i]
	// numTasks[i]				= total tasks to be executed by throttlers[i]
	duration unitTimes[numThrottlers] = {oneSec, oneSec, oneSec, oneSec};
	size_t bandWidths[numThrottlers] = {1000, 2000, 30000, 4000};
	std::vector<time_point> execTimeStamps[numThrottlers];
	size_t numTasks[numThrottlers];

	// total tasks to be executed by all the throttlers combined
	size_t totalTasks = 0;
	for (size_t i = 0; i < numThrottlers; ++i)
	{
		// no. of tasks fo each throttler = bandWidth * 10
		// so all the tasks should be executed in unitTime * 10 seconds, i.e 10 sec
		// as the unitTime for each thread is kept to be 1 sec
		numTasks[i] = bandWidths[i] * 10;
		totalTasks += bandWidths[i] * 10;
	}

	std::unique_ptr<mt::ReusableThrottledWorkerThread> throttlers[numThrottlers];

	for (size_t i = 0; i < numThrottlers; ++i)
	{
		throttlers[i] = std::move(std::make_unique<mt::ReusableThrottledWorkerThread>(worker, scheduler, unitTimes[i], bandWidths[i]));
	}

	mtInternal::ConditionVariable cond;

	// Total tasks executed yet
	size_t taskExecutionCounter = 0;

	// Execute the task and add the time of execution to the execLog
	auto addExecTimeStamp = [this, &cond, &taskExecutionCounter, &totalTasks](std::vector<time_point> &execTimestamp)
	{
		auto now = ULCommonUtils::now();
		execTimestamp.push_back(now);
		// Using rand here to avoid compiler optimizations which may lead to a non-linear execution
		rand();
		taskExecutionCounter++;

		// If all the tasks have been executed, signal the cond variable
		if (totalTasks == taskExecutionCounter)
			cond.notify_one();
	};

	// push 'numTasks' tasks to the provided throttler
	// and update its execution log
	auto pushTasks = [&addExecTimeStamp](mt::ReusableThrottledWorkerThread &throttler,
																			 const size_t &numTasks,
																			 std::vector<time_point> &execTimeStamps)
	{
		for (int i = 0; i < numTasks; i++)
		{
			throttler.push([&addExecTimeStamp, &execTimeStamps]()
										 { addExecTimeStamp(execTimeStamps); });
		}
	};

	std::thread pusherThreads[numThrottlers];
	for (size_t i = 0; i < numThrottlers; ++i)
	{
		pusherThreads[i] = std::thread([&, i](){
			pushTasks(*throttlers[i], numTasks[i], execTimeStamps[i]);
		});
	}

	for (size_t i = 0; i < numThrottlers; ++i)
	{
		pusherThreads[i].join();
	}

	cond.wait();
	ASSERT_EQ(taskExecutionCounter, totalTasks);

	for (size_t i = 0; i < numThrottlers; ++i)
	{
		ASSERT_EQ(execTimeStamps[i].size(), numTasks[i]);
	}

	// Validate that each throttler executed exactly the no. of tasks it delegated
	for (size_t i = 0; i < numThrottlers; ++i)
	{
		ASSERT_EQ(execTimeStamps[i].size(), numTasks[i]);
	}

	// Time to validate the execLog of each throttler
	// Following points are validated in this function:
	
	auto validateTransactionLog =
			[](const std::vector<time_point> &execLog,
				 const duration &unitTime,
				 const size_t &bandWidth,
				 const size_t &totalTasks)
	{
		// no. of time windows of size 'unitTime' encountered while parsing the execLog
		size_t numSections = 0;
		for (auto [timeWindowStart, timeWindowEnd, startIndex, endIndex] = std::tuple{execLog[0], execLog[0] + unitTime, 0, 1};
				 endIndex <= execLog.size();
				 endIndex++)
		{
			// End of execLog encountered
			if (endIndex == execLog.size())
			{
				// Each 'unitTime' time window should have exactly 'bandWidth' entries
				ASSERT_EQ(endIndex - startIndex, bandWidth);

				// The timespan in which 'bandWidth' tasks excuted should be <= 'unitTime'
				// This is to endure that the throttler did not 'over-throttle' the tasks
				ASSERT_LE(execLog[endIndex - 1] - execLog[startIndex], unitTime);
				++numSections;
			}
			// Curr time window >= unitTime
			else if (execLog[endIndex] >= timeWindowEnd)
			{
				ASSERT_EQ(endIndex - startIndex, bandWidth);
				ASSERT_LE(execLog[endIndex - 1] - execLog[startIndex], unitTime);
				timeWindowStart += unitTime;
				timeWindowEnd = timeWindowStart + unitTime;
				startIndex = endIndex;
				++numSections;
			}
		}

		// numSections should be = totalTasks / bandWidth if every thing was handled perfectly
		// For example if the bandWidth was 1000 and the unitTime was 1 sec
		// Then if i pushed 15000 tasks i should have 15000/1000 = 15 sections of 1000 entries
		// in the execLog
		ASSERT_EQ(numSections, (size_t)(totalTasks / bandWidth));
	};

	for (size_t i = 0; i < numThrottlers; ++i)
	{
		validateTransactionLog(execTimeStamps[i], unitTimes[i], bandWidths[i], numTasks[i]);
	}
}

int main(int argc, const char **argv)
{
	::testing::InitGoogleTest();
	return RUN_ALL_TESTS();
}
