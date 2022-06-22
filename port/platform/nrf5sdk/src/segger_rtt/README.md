# Why These Files Are Here
These files are taken from the SEGGER RTT source examples.  They are here because:

1.  In order not to lock interrupts out for the sake of logging we modify the locks around the logging parts to be RTOS locks rather than critical section locks.
2.  In the Nordic version of `SEGGER_RTT_Conf.h` `SEGGER_RTT_LOCK()`/`SEGGER_RTT_UNLOCK()` are defined directly (i.e. without `#ifdef`s around them) to be critical section locks; hence we need to use a modified version of `SEGGER_RTT_Conf.h`.
3.  The inclusion of `SEGGER_RTT_Conf.h`, via `SEGGER_RTT.h`, is done with quoted includes rather than angle-bracket includes, which means that a header file found in the same directory takes priority, irrespective of the include paths passed to the compiler, and all the `.c` and `.h` files are in the same directory.
4.  Hence, to modify `SEGGER_RTT_LOCK()`/`SEGGER_RTT_UNLOCK()`, the `SEGGER_RTT_Syscalls_*.c` files have to be copied locally, so we might as well use the oiginal SEGGER version and avoid getting tangled up in nRF5 SDK \#defines.