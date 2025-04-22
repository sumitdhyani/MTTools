#include <vector>
#include <CommonUtils/CommonDefs.hpp>
#include <Timer.hpp>
#include <gtest/gtest.h>

namespace mt = ULMTTools;
namespace utils = ULCommonUtils;

struct BasicTimerTests : ::testing::Test
{
	std::vector<time_point> taskExecutionTimestamps;
	stdMutex mutex;

	virtual void setup()
	{
		taskExecutionTimestamps.clear();
	}
};

struct AdvancedTimerTests : ::testing::Test
{
	stdMutex mutex;
	std::vector<time_point> taskExecutionTimestamps1;
	std::vector<time_point> taskExecutionTimestamps2;
	std::vector<time_point> taskExecutionTimestamps3;

	virtual void setup()
	{
		taskExecutionTimestamps1.clear();
		taskExecutionTimestamps2.clear();
		taskExecutionTimestamps3.clear();
	}
};

TEST_F(BasicTimerTests, Basic)
{
	mt::Timer timer(std::make_shared<mt::TaskScheduler>());

	auto func = [this]()
	{
		auto now = utils::now;
		stdUniqueLock lock(mutex);
		taskExecutionTimestamps.push_back(now());
	};

	auto id = timer.install(func, std::chrono::seconds(1));

	// Sleep for 10.5 sec
	std::this_thread::sleep_for(std::chrono::milliseconds(10500));
	
	{
		stdUniqueLock lock(mutex);
		ASSERT_EQ(taskExecutionTimestamps.size(), 11);
	}

	std::this_thread::sleep_for(std::chrono::seconds(5));
	timer.unInstall(id);
	ASSERT_EQ(taskExecutionTimestamps.size(), 16);
}


TEST_F(AdvancedTimerTests, MultipleTimers)
{
	mt::Timer timer(std::make_shared<mt::TaskScheduler>());

	auto func = [this](std::vector<time_point>& taskExecutionTimestamps)
	{
		auto now = utils::now;
		stdUniqueLock lock(mutex);
		taskExecutionTimestamps.push_back(now());
	};


	duration p1 = std::chrono::milliseconds(1000);
	duration p2 = p1 * 2;
	duration p3 = p1 * 3;

	size_t id1 = timer.install(std::bind(func, std::ref(taskExecutionTimestamps1)), p1);
	size_t id2 = timer.install(std::bind(func, std::ref(taskExecutionTimestamps2)), p2);
	size_t id3 = timer.install(std::bind(func, std::ref(taskExecutionTimestamps3)), p3);

	// Sleep for 31.5 sec
	std::this_thread::sleep_for(p1*30 + std::chrono::milliseconds(500));
	
	{
		stdUniqueLock lock(mutex);
		ASSERT_EQ(taskExecutionTimestamps1.size(), 31);
		ASSERT_EQ(taskExecutionTimestamps2.size(), 16);
		ASSERT_EQ(taskExecutionTimestamps3.size(), 11);
	}

	std::this_thread::sleep_for(p1*12);
	timer.unInstall(id1);
	timer.unInstall(id2);
	timer.unInstall(id3);
	ASSERT_EQ(taskExecutionTimestamps1.size(), 43);
	ASSERT_EQ(taskExecutionTimestamps2.size(), 22);
	ASSERT_EQ(taskExecutionTimestamps3.size(), 15);
}


TEST_F(AdvancedTimerTests, MultipleTimersFromMultipleThreads)
{
	mt::Timer timer(std::make_shared<mt::TaskScheduler>());

	auto func = [this](std::vector<time_point>& taskExecutionTimestamps)
	{
		auto now = utils::now;
		stdUniqueLock lock(mutex);
		taskExecutionTimestamps.push_back(now());
	};

	size_t id1;
	size_t id2;
	size_t id3;

	duration p1 = std::chrono::milliseconds(1000);
	duration p2 = p1 * 2;
	duration p3 = p1 * 3;

	std::thread t1([this, &timer, &func, &id1, p1]() {id1 = timer.install(std::bind(func, std::ref(taskExecutionTimestamps1)), p1); });
	std::thread t2([this, &timer, &func, &id2, p2]() {id2 = timer.install(std::bind(func, std::ref(taskExecutionTimestamps2)), p2); });
	std::thread t3([this, &timer, &func, &id3, p3]() {id3 = timer.install(std::bind(func, std::ref(taskExecutionTimestamps3)), p3); });

	t1.join();
	t2.join();
	t3.join();

	std::this_thread::sleep_for(p1*30 + std::chrono::milliseconds(500));
	
	{
		stdUniqueLock lock(mutex);
		ASSERT_EQ(taskExecutionTimestamps1.size(), 31);
		ASSERT_EQ(taskExecutionTimestamps2.size(), 16);
		ASSERT_EQ(taskExecutionTimestamps3.size(), 11);
	}

	std::this_thread::sleep_for(p1*12);
	timer.unInstall(id1);
	timer.unInstall(id2);
	timer.unInstall(id3);
	ASSERT_EQ(taskExecutionTimestamps1.size(), 43);
	ASSERT_EQ(taskExecutionTimestamps2.size(), 22);
	ASSERT_EQ(taskExecutionTimestamps3.size(), 15);
}

int main(int argc, const char **argv)
{
	::testing::InitGoogleTest();
	return RUN_ALL_TESTS();
}
