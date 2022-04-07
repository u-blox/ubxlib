# Introduction
The files here provide utility functions that allow a mutex dead-lock within the `ubxlib` code to be debugged.

Mutex dead-locks are notoriously difficult to debug: they are dynamic in nature, very sensitive to timing, and what constitues a "stuck" mutex is absolutely application/situation specific.  They can happen once in a blue moon but, once they have happened, they stop the tasks in question dead and normal watchdog timers won't save the day (since interrupts are still running and tasks are still being scheduled normally).  Hence, if you suspect that your system is stopping because of a mutex deadlock, it may be worth compiling in this extra code and then running and running and running your system in the hope of capturing the problem.

# Usage
This code works by replacing the usual [port](/port/api) OS mutex functions with macros (so that the file name and line number are known), renaming the usual functions to have an "\_" at the start.  The macros then call the intermediate functions here which keep track of where mutexes are created and locked before calling the original "\_" functions.  A "mutex watchdog" task then checks periodically for how long any one caller has been waiting on a lock and, if that exceeds a certain time, it calls a callback of your choice; one callback is provided which prints out the file and line numbers of where each extant mutex was created, locked and is being waited on.

If you find that checking on the length of waiting time doesn't work for your particular problem you could modify the code in the mutex watchdog task to check other criteria.

To run your code with mutex debug, simply define `U_CFG_MUTEX_DEBUG` for your build.  Read the comments at the top of [u_mutex_debug.h](u_mutex_debug.h) for more information.

IMPORTANT: in order to support this debug feature, it must be possible on your platform for a task and a mutex to be created **before** `uPortInit()` is called, right at start of day, and such a task/mutex must also survive `uPortDeinit()` being called.  This is because `uMutexDebugInit()` must be able to create a mutex and `uMutexDebugWatchdog()` must be able to create a task and these must not be destroyed for the life of the application.