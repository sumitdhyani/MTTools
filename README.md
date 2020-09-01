Provides object oriented interface for multithreading applications.
The applictions should imclude header WorkerThread.hpp.
Following are the classes which may be used by the client applications:
1. WorkerThread: Async task executor
2. TaskScheduler: Used for timed execution of tasks, executes tasks in its own thread
3. ThrottledWorkerThread: Asynctask executor, with limit of executing certain no. of tasks/unit time, the unittime and the no. of tasks are given during its construction, each object runs in a separate thread
4. ReusableThrottledWorkerThread: A ThrottledWorkerThread in which many objects can run in shared threads, useful where there are already many threads in the application, so the context switching is significant, or there are many bandwidths to be maintained requiring a lot of throttler objects. So the application can maintain each bandwidth using a separate object but all of them sharing the same threads. Requires a WorkerThread and a TaskScheduler for its construction.
