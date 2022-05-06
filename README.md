Provides object oriented interface for multithreading applications.
The applictions should imclude header WorkerThread.hpp.
Following are the classes which may be used by the client applications:

1. WorkerThread: An interface to execute tasks in a separate thread, all tasks executed using this interface are guarenteed to execute in the same thread.
See unitTests/WorkerThreadTests.cpp for examples.

2. TaskScheduler: Used for timed execution of tasks, executes tasks in its own thread

3. Timer: Used to repeatedly execute a task/function, for example client sending hertbeat messages to server.
See unitTests/TimerTests for examples.

4. ThrottledWorkerThread: Asynchronous task executor, with limit of executing certain no. of tasks/unit time, the unittime and the no. of tasks are given during its construction, each object runs in a separate thread.
See unitTests/ThrottlingTests.cpp for examples.

5. ReusableThrottledWorkerThread: A ThrottledWorkerThread in which many objects can run in shared threads, useful where there are already many threads in the application, so the context switching is significant, or there are many bandwidths to be maintained requiring a lot of throttler objects. So the application can maintain each bandwidth using a separate object but all of them sharing the same thread. Requires a WorkerThread and a TaskScheduler for its construction.
See unitTests/ThrottlingTests.cpp for examples.

6. ThreadPool: An interface to execute tasks parallely.

* In this file the term "task" refers to an equivalent of a function with prototype void().
* This can be a std::function<void()> object, a lambda, a function pointer or a functor.
