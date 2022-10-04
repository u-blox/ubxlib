# Introduction
This component provides a simple, fast, binary logging facility that can be useful when debugging difficult real-time problems, i.e. ones where break-pointing in a debugger is of no use, you need a detailed real-time log that doesn't overload the system (as a `uPortLog()` would).  It is derived from the log client that can be found [here](https://github.com/u-blox/log-client).

It should _NOT_ be included in core `ubxlib` code - simply bring it into play where required when debugging on a branch and take it out again before your code is merged.

Each log entry contains three things:

- a millisecond timestamp (32 bits),
- the logging event that occurred (32 bits),
- a 32 bit integer carrying further information about the logging event.

Functions are provided to retrieve log entries and to print out the log.

# Usage
The pattern of usage is as follows:

- If you wish, add your named log events to the files:
  - [u_log_ram_enum_user.h](u_log_ram_enum_user.h)
  - [u_log_ram_string_user.h](u_log_ram_string_user.h)

  You may also just employ the existing generic user events in [u_log_ram_enum.h](u_log_ram_enum.h).

- Place calls to `uLogRam()` anywhere in your code where you wish to log an event.  For instance, if you have defined a log event `U_LOG_RAM_EVENT_BATTERY_VOLTAGE` then you could log the event with the battery voltage as the parameter as follows: 

`uLogRam(U_LOG_RAM_EVENT_BATTERY_VOLTAGE, voltage);`

  By convention, if no parameter is required for a log item then 0 is used.

- Near the start of your code, add a call to `uLogRamInit()`, passing in a pointer to a logging buffer of size `U_LOG_RAM_STORE_SIZE` bytes, or passing `NULL` to have it `malloc()` logging space for you; logging will begin at this point.
- To print out the logging data that has been captured since `uLogRamInit()`, call `uLogRamPrint()`.
- Your code may also call `uLogRamGet()` to retrieve log items (in FIFO order) from RAM storage, removing them from the store.
- When logging is to be stopped, call `uLogRamDeinit()`; if you passed a buffer to `uLogRamInit()` the contents of that buffer will still be available for examination aftewards but if you let `uLogRamInit()` `malloc()` logging space then calling `uLogRamDeinit()` will deallocate it, it will no longer be printable; in the usual case, when you are just hacking in some temporary debug, you'll probably not bother calling `uLogRamDeinit()`.

Note: there is no mutex protection on the `uLogRam()` call since the priority is to log quickly and efficiently.  Hence it is possible for two `uLogRam()` calls to collide resulting in those particular log calls being mangled.  This will happen very rarely (I've never seen it happen in fact) but be aware that it is a possibility.  If you don't care about speed so much then call `uLogRamX()` instead; this _will_ mutex-lock.
