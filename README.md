# MTTools: A library to that provides an easy interface for Multi-threading utilities
Provides object oriented interface for multithreading applications. The library is header only.

## Build:

**There is nothing to build as the whole library is header-only if you don't want to build/run Unit Tests**

## To build unit tests, following are the prerequisites:
- CMake(3.5.0+)
- GTest, environment variable **GTEST_ROOT** should be set to the googletest source directory root
- On windows clang with C++20 support
- On linux g++ with C++20 support
  
If all the prerequisites are met then just go to the root directory and run the setup.sh/setup.bat for linux/windows and you're ready to go.

## Overview of functionalities provided:
### Following are the classes which may be used by the client applications:
  - **WorkerThread:**
    - An interface to execute tasks in a separate thread, all tasks executed using this interface are     guaranteed to execute in the same thread. See unitTests/WorkerThreadTests.cpp for examples.
  - **TaskScheduler:**
    -  Used for timed execution of tasks, executes tasks in its own thread
  - **Timer:**
    - Used to repeatedly execute a task/function, for example client sending heartbeat messages to server. See unitTests/TimerTests for examples.
  - **ThrottledWorkerThread:**
    - Asynchronous task executor, with limit of executing certain no. of tasks/unit time, the unit time and the no. of tasks are given during its construction, all the tasks pushed to  this interface run in the same thread, which is owned by the "ThrottledWorkerThread" object. See unitTests/ThrottlingTests.cpp for examples.
  - **ReusableThrottledWorkerThread:**
    - A ThrottledWorkerThread in which many objects can run in shared threads, useful where there are already many threads in the application, so the context switching is significant, or there are many bandwidths to be maintained requiring a lot of throttler objects. So the application can maintain each bandwidth using a separate object but all of them sharing the same thread. Requires a WorkerThread and a TaskScheduler for its construction. See unitTests/ThrottlingTests.cpp for examples.
  - **ThreadPool:**
    - An interface to execute tasks parallelly.