# Introduction
Some hints on how to go about debugging `ubxlib`, aimed at `ubxlib` developers and at customers writing applications that call `ubxlib`.

# Reading Error Return Codes
`ubxlib` provides optional feedback to the developer through debug prints that may be enabled or disabled but it _always_ provides feedback to the _application_ through `int32_t` return codes, where negative means fail; checking these will usually tell you what might be going wrong, without the overhead of debug prints.

99.99% of the time the error code will be one from [u_error_common.h](/common/error/api/u_error_common.h); check the negative number you have received against that file: e.g. `U_ERROR_COMMON_INVALID_PARAMETER` (-5), `U_ERROR_COMMON_NOT_SUPPORTED` (-4), `U_ERROR_COMMON_TIMEOUT` (-9), etc.  The other 0.01% of the time the error code will be technology-specific, see near the top of [u_cell.h](/cell/api/u_cell.h), [u_gnss.h](/gnss/api/u_gnss.h), [u_wifi.h](/wifi/api/u_wifi.h), [u_ble.h](/ble/api/u_ble.h) or [u_short_range.h](/common/short_range/api/u_short_range.h); these technology-specific error codes are separated into distinct number ranges (also identified in [u_error_common.h](/common/error/api/u_error_common.h)) so that it is easy to tell which is which (e.g. `U_GNSS_ERROR_NACK` is -1025, in the range -1024 to -2047).

# Enabling Debug Prints
**ALL** of the log items from here onwards are emitted by the `uPortLog()` variadic (i.e. like `printf()`) macro.  Logging is enabled at compile-time by setting `U_CFG_ENABLE_LOGGING` to 1 in [u_cfg_sw.h](/cfg/u_cfg_sw.h); it is 1 by default, you may override it by passing `U_CFG_ENABLE_LOGGING=0` into your build.  With logging enabled at compile-time, it may be disabled at run-time by calling `uPortLogOff()` and re-enabled again at run-time by calling `uPortLogOn()`.

# AT Interface Debugging
To debug interactions with cellular or short-range devices, the first step is usually to switch-on printing of the AT-command exchange with the device.  If you opened the device with the `uDevice` API these prints will be on by default, else you may enable them by calling `uAtClientPrintAtSet()` with `true`.  This will print _purely_ the AT-command exchange; to also print a small amount of additional behavioural debug from the AT Client code you may call `uAtClientDebugSet()` with `true`.

If you intend to take a long-term log of an unattended device you may also wish to call `uAtClientTimestampSet()`, either with the current Unix timestamp or with zero, to add a timestamp to every AT Client log print.

If you suspect an issue with control characters on the AT interface, you may cause these to be printed as hex in the log output by defining `U_AT_CLIENT_PRINT_CONTROL_CHARACTERS` in your build.

# GNSS Interface Debugging
Similar to the AT Client, to debug interactions with GNSS you will want to print the UBX message exchanges with the device; if you opened the GNSS device/network with the `uDevice`/`uNetwork` API these will be on by default, else you may enable them by calling `uGnssSetUbxMessagePrint()` with `true`.  There is also a Python script in the [gnss/api](/gnss/api) directory, [u_gnss_ucenter_ubx.py](/gnss/api/u_gnss_ucenter_ubx.py), which can parse the `ubxlib` log output looking for the `UBX` messages passed between the GNSS device and this MCU and put them into a file, or pass them out of a serial port, that can be read by the u-blox [uCenter tool](https://www.u-blox.com/en/product/u-center); see the [README.md](/gnss/api) in that directory for how to install and use it.

# Adding Your Own Debug Prints
You may add your own `uPortLog()` calls to any `.c` file to print interesting stuff; if you are doing this to a `.c` file within `ubxlib` itself, make sure that [u_port_debug.h](/port/api/u_port_debug.h) is included in the `.c` file and, before it, [u_cfg_sw.h](/cfg/u_cfg_sw.h), otherwise the `uPortLog()` macro will do nothing for you (within `ubxlib` we include only the required headers in each `.c` file, rather than including everything via `ubxlib.h`, and if the `.c` file originally contained no prints the headers won't have been included).

Of course, adding such debug prints may affect timing, so beware.

# Tracking Heap And OS Resource Allocations
Heap allocations, creation of OS resources (tasks, queues, mutexes, semaphores and timers) and opened transports (UART, I2C, SPI) are all tracked continuously without the need to supply any conditional compilation flags to your build.  To keep things simple/quick, the accounting is done with a few counters:

- `uPortHeapAllocCount()` prints the number of outstanding heap allocations (i.e. the number of `pUPortMalloc()` calls minus the number of `uPortFree()` calls); this is sufficient since `ubxlib` does not use `realloc()`, 
- `uPortOsResourceAllocCount()` prints the number of outstanding OS resources (tasks, queues, mutexes, semaphores and timers),
- `uPortUartResourceAllocCount()` prints the number of open UARTs, 
- `uPortI2cResourceAllocCount()` prints the number of open I2C devices, 
- `uPortSpiResourceAllocCount()` prints the number of open SPI devices.

There are places in the `ubxlib` code where an OS resource, usually a mutex, is created and will never be deleted in order to ensure thread-safety.  These situations are also counted; call `uPortOsResourcePerpetualCount()` to find out how many from your `uPortOsResourceAllocCount()` are of this nature.

See below for how to find out _where_ an OS resource or heap memory leak occurred.

# Locating A Heap Memory Leak
`uPortHeapAllocCount()` will tell you if there is a heap allocation outstanding but not who nabbed it; to find this out, add the conditional compilation flag `U_CFG_HEAP_MONITOR` to your build and, near the end of your program, call `uPortHeapDump()` to get a printed list of what is outstanding and where it was allocated.  As well as tracking the allocations/frees, `U_CFG_HEAP_MONITOR` adds guards to each heap memory allocation and checks them when `uPortFree()` is called; should there be corruption, `U_ASSERT()` is called with `false`.

# Locating An OS Resource Leak
`uPortOsResourceAllocCount()` will tell you how may OS resources are outstanding but not what type or who allocated them.  To determine this, add the conditional compilation flag `U_PORT_OS_DEBUG_PRINT` to your build.  This will cause debug prints of the following form to be output whenever an OS resource is created or deleted:

- `U_PORT_OS: +T 00001600 "blah" stack 1156 priority 12`: task created at address 0x00001600,
- `U_PORT_OS: +Q 00000007 length 20 item size 8`: queue created at address 0x00000007,
- `U_PORT_OS: +M 000001B4`: mutex created at address 0x000001B4,
- `U_PORT_OS: +S 00000178 initial count 0 limit 20`: semaphore created at address 0x00000178,
- `U_PORT_OS: +t 000002CC "timer 1" interval 1000 one-shot`: timer created at address 0x000002CC,
- `U_PORT_OS: -T 00001600`: task at address 0x00001600 deleted,
- `U_PORT_OS: -Q 00000007`: queue at address 0x00000007 deleted,
- `U_PORT_OS: -M 000001B4`: mutex at address 0x000001B4 deleted,
- `U_PORT_OS: -S 00000178`: semaphore at address 0x00000178 deleted,
- `U_PORT_OS: -t 000002CC`: timer at address 0x000002CC deleted.

You might load this output into an editor such as [Notepad++](https://notepad-plus-plus.org/downloads/), capable of highlighting all occurrences of a word, i.e. a hex address, and visually check that the same address appears later in the same log (meaning the OS resource created at that address was deleted); that, or count the number of occurrences of a given address in the log and, if it is even, that OS resource was deleted; or, of course, get ChatGPT to write a fancy-pants script to do this for you :-).  From the surrounding log context it is then usually possible to find out where in the code the OS resource was created.

There may be occasions where a print has been emitted by an asynchronous task and hence may be broken up by other prints, so a text search won't find it; when you have found your suspect you should confirm by eye that it hasn't fallen into this checking-hole.  Note that the use of these macros obviously affects timing etc.; do not use them routinely.

Implementation note: OS resource tracking is done in this simple way, versus the linked-list mechanism employed for heap memory allocation tracking, as it is (a) usually sufficient and (b) doesn't make a horrible macroized mess of the `uPort` OS API.

# Tracking Heap Usage
Depending on your platform, you may also be able to obtain the current free heap by calling `uPortGetHeapFree()` and get the low-watermark of the heap by calling `uPortGetHeapMinFree()`; these functions will return a negative error code if they are not supported on your platform.  They are currently supported by STM32Cube, ESP-IDF (including Arduino flavour) and nRF5SDK.

# Tracking Stack Usage
Depending on your platform, you may also be able to obtain the stack low-watermark for the current task by calling `uPortTaskStackMinFree(NULL)`; the `runner` build for each platform, which runs all of our testing, is configured such that the main application task should always have a minimum of `U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES` (5120) of stack free and this is checked at the end of every test.  Checking the stack low-watermark is currently supported by Zephyr, STM32Cube, ESP-IDF (including Arduino flavour) and nRF5SDK; `uPortTaskStackMinFree()` will return a negative error code if it is not supported on your platform.

The same principle applies to all things within `ubxlib` that use tasks: see for instance `uPortEventQueueStackMinFree()`, `uPortUartEventStackMinFree()`, `uAtClientCallbackStackMinFree()`, etc.; search recursively for "StackMinFree" in `*.h` of `ubxlib` to find all of the instances.

Should you suspect that anything is running out of stack you may add calls to these functions to find out how close things are.

# Locating A Mutex Lock-Up
`ubxlib` is designed to be thread-safe and hence makes frequent use of mutexes, all of which are simple non-recursive mutexes.  A down-side of this is that it is possible for code to "get stuck" waiting for a mutex which will never be unlocked.

To detect such lock-ups, add the conditional compilation flag `U_CFG_MUTEX_DEBUG` to your build, include [u_mutex_debug.h](/port/platform/common/mutex_debug/u_mutex_debug.h) in your application and, somewhere near the start, add calls to `uMutexDebugInit()` followed by `uMutexDebugWatchdog(uMutexDebugPrint, NULL, U_MUTEX_DEBUG_WATCHDOG_TIMEOUT_SECONDS)`; you can find an example of where we set this up for testing in the file `app/u_main.c` for your platform.  This will track the locking/unlocking of all mutexes and, if anything has been waiting on a mutex for more than `U_MUTEX_DEBUG_WATCHDOG_TIMEOUT_SECONDS` (60) seconds, the current state of all mutexes will be printed, for example:

```
U_MUTEX_DEBUG_0x2000e960: created by C:/projects/ubxlib/port/test/u_port_test.c:2053 approx. 60 second(s) ago is LOCKED.
U_MUTEX_DEBUG_0x2000e960: locker has been C:/projects/ubxlib/port/test/u_port_test.c:2053 for approx. 60 second(s).
U_MUTEX_DEBUG_0x2000e960: C:/projects/ubxlib/port/test/u_port_test.c:2055 has been **WAITING** for a lock for approx. 60 second(s).
U_MUTEX_DEBUG_0x2000a7e8: created by C:/projects/ubxlib/port/platform/stm32cube/src/u_port_uart.c:892 approx. 60 second(s) ago is not locked.
U_MUTEX_DEBUG_0x2000a840: created by C:/projects/ubxlib/port/platform/common/event_queue/u_port_event_queue.c:229 approx. 60 second(s) ago is not locked.
```

Look for lines with `**WAITING**` in them to find the lines of code that have been waiting for more then 60 seconds for a lock (there may be one, sometimes two); the log-line above will tell you the line of code that has the mutex already locked.

For more details see [mutex_debug](/port/platform/common/mutex_debug).

# Thread Dumper
As well as the mutex lock-up detector, on some platforms (currently ESP-IDF and Zephyr on 32-bit Thumb) it is possible to add an inactivity detector which will print out the list of active tasks and their backtrace if `uPortLog()` is not called for a given interval.

To use this define `U_DEBUG_UTILS_DUMP_THREADS` for your build; in your main application, declare the `extern` variable `extern volatile int32_t gStdoutCounter` and, near the start of your application, add a call to `uDebugUtilsInitInactivityDetector(&gStdoutCounter)`; you can find an example of where we set this up for testing in the file `app/u_main.c` for your platform.

Then if nothing calls `uPortLog()` for `U_DEBUG_UTILS_INACTIVITY_TASK_CHECK_PERIOD_SEC` (180) seconds you will get a backtrace for all active tasks.

For more details see [debug_utils](/port/platform/common/debug_utils).

# Decoding Backtrace With GCC
Should backtrace by emitted by a platform (e.g. on a panic) that is _not_ decoded, i.e. it just consists of hex addresses, then provided the application was compiled with GCC you may locate the GCC executable `addr2line` (in the same directory as the compiler you compiled the code with) and, with the `.elf` file for your build (which must include debug information, the `-g` flag for GCC), decode the backtrace, line by line, yourself with something like:

```
addr2line.exe -e my_app.elf 0x400d3e6f
```

...where `0x400d3e6f` is the last address in the backtrace; repeat until done.

# Debugging A U_ASSERT
There are a few [U_ASSERT()](/common/assert) checks in the `ubxlib` code; deliberately few: as a library we prefer return-values that give the application control, to going "bang" in an unstructured fashion.  You may add your own assert function at run-time, by calling `uAssertHookSet()`, to make more sense of what might be going on; if you do not do so, an assertion will print a useful message (through `uPortLog()`) and the code will sit in an infinite loop.

If you add your own assert function then, during testing, you may also chose to define `U_ASSERT_HOOK_FUNCTION_TEST_RETURN` for your build; in this case code execution will continue as normal after the assertion.  To be clear, `U_ASSERT_HOOK_FUNCTION_TEST_RETURN` ONLY takes effect if you have added your own assert function, it does nothing if the default assert function is in use, and you should _really_ only use it when testing as assertions indicate that the world has likely fallen apart; you do not normally want to continue.

# RAM Logging
In real-time systems it is often the case that a debugger is damn-all use: the last thing you want is for your code to stop, you want a record of what it did in real-time but with no run-time impact (so a `uPortLog()`-style `printf()` is of no use either).  For these ticklish real-time cases, you may instrument the code with calls to a RAM logger.

To do this, include [u_log_ram.h](/port/platform/common/log_ram/u_log_ram.h) in the `.c` file(s) you wish to debug and, near the start of your application, add a call to `uLogRamInit()`, passing in a pointer to a logging buffer of size `U_LOG_RAM_STORE_SIZE` bytes, or passing `NULL` to have `uLogRamInit()` `pUPortMalloc()` logging space for you; logging will begin at this point.

Place calls to `uLogRam()` anywhere in the `.c` file(s) where you wish to log an event; doing this in an interrupt or critical section is fine, no holds barred.  For instance:

`uLogRam(U_LOG_RAM_EVENT_USER_0, x);`

This will record a millisecond timestamp (32 bits), the logging event that occurred (in this case `U_LOG_RAM_EVENT_USER_0`) and the value of `x` (32 bits).  By convention, if no parameter is required for a log event then 0 should be used.  

Near the end of your application, or wherever you want to dump (and empty) the contents of the RAM log, call `uLogRamPrint(NULL)`.

Events `U_LOG_RAM_EVENT_USER_0` to `U_LOG_RAM_EVENT_USER_9` are provided; you may add your own RAM log events in [u_log_ram_enum_user.h](/port/platform/common/log_ram/u_log_ram_enum_user.h)/[u_log_ram_string_user.h](/port/platform/common/log_ram/u_log_ram_string_user.h) if the problem is particularly complex.

For more details see [log_ram](/port/platform/common/log_ram).

# Visual Studio Code
You may find [Visual Studio Code](https://code.visualstudio.com/) a convenient IDE and/or debug environment.  There are a few ways to use it with `ubxlib`:

- If you wish to run the `ubxlib` tests on Linux or Windows, with an EVK or two connected via USB, just open the relevant `runner` folder ([port/platform/linux/mcu/posix/runner](/port/platform/linux/mcu/posix/runner) or [port/platform/windows/mcu/win32/runner](/port/platform/windows/mcu/win32/runner)) in [Visual Studio Code](https://code.visualstudio.com/) and it should tell you what additional things you need to install (GCC and CMake for Linux, MSVC and CMake for Windows).
- If you wish to run the `ubxlib` tests on an MCU, open [ubxlib-runner.code-workspace](/ubxlib-runner.code-workspace) in [Visual Studio Code](https://code.visualstudio.com/), select the "Run and Debug" icon on the far left and then select an option from the "RUN AND DEBUG" pull-down on the top left.

From these cases you should be able to figure out how to use [Visual Studio Code](https://code.visualstudio.com/) with `ubxlib` for your own application; likely a lot simpler if you have chosen a single platform and aren't running the `ubxlib` tests.