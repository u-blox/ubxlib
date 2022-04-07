# Introduction
This folder contains common debug utilities. At the moment it is only used for dumping thread status. Please note that this is currently only supported for FreeRTOS and Zephyr running on Cortex-Mx compiled with GCC.

## Thread Dumper Usage
Currently the thread dumper only supports ARM in Thumb mode on FreeRTOS or Zephyr. To enable the thread dumper you need to define `U_DEBUG_UTILS_DUMP_THREADS`. When enabled you can call `uDebugUtilsDumpThreads()` to print out a thread dump. This will look something like this:

```
### Dumping threads ###
  timerEvent (pending): bottom: 200064e0, top: 20006ce0, sp: 20006bd8
    Backtrace: 0x00050e16 0x0004e68a 0x0005c910 0x0005a1b6 0x0005a196 0x0005a196 0x0005d724
  sysworkq (pending): bottom: 200289a0, top: 200291a0, sp: 20029120
    Backtrace: 0x00050e16 0x000525d4 0x0004fe8c 0x0005d724 
  MPSL signal (pending): bottom: 2002c420, top: 2002c800, sp: 2002c790
    Backtrace: 0x00050e16 0x0004f91c 0x00049744 0x0005d724 
  idle 00 (): bottom: 20028020, top: 20028160, sp: 20028138
    Backtrace: 
  main (queued): bottom: 2002a020, top: 2002c000, sp: 2002bf38
    Backtrace: 0x0004200a 0x0005c586 0x0001e68e 0x0001e6ea 0x0004d8e0 0x0001e704 0x0003e1aa 0x0005c4d4 0x0003e21e 0x0004d048 0x0005d724 0x00041014 
```

The back traces can then be converted to source code lines with line numbers by the use of `addr2line` (for ARM that would be `arm-none-eabi-addr2line'). If you use our PyInvoke commands for viewing the output we will convert the back trace lines automatically if you have specified the .elf file. In this case the above output will look like this:

```
  timerEvent (pending): bottom: 200064e0, top: 20006ce0, sp: 20006bd8
    0x00000bac: ??                           :?
    0x00050e16: z_swap_irqlock               /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/kernel/include/kswap.h:162
     (inlined by) z_swap                     /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/kernel/include/kswap.h:173
     (inlined by) z_pend_curr                /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/kernel/sched.c:709
    0x0004e68a: z_impl_k_msgq_get            /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/kernel/msg_q.c:260
    0x0005c910: k_msgq_get                   /home/anan/git/ubxlib_priv/.vscode/_build/nrfconnect/runner_ubx_evkninab3_nrf52840/zephyr/include/generated/syscalls/kernel.h:853
     (inlined by) uPortQueueReceive          /home/anan/git/ubxlib_priv/port/platform/zephyr/src/u_port_os.c:417
    0x0005a1b6: disassembly_ins_is_bl_blx    /home/anan/git/ubxlib_priv/port/platform/common/debug_utils/src/zephyr/../arch/arm/u_print_callstack_cortex.c:80 (discriminator 1)
    0x0005a196: uMemPoolFreeAllMem           /home/anan/git/ubxlib_priv/common/utils/src/u_mempool.c:234
    0x0005a196: uMemPoolFreeAllMem           /home/anan/git/ubxlib_priv/common/utils/src/u_mempool.c:234
    0x0005d724: z_thread_entry               /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/lib/os/thread_entry.c:29
    Backtrace: 0x00050e16 0x0004e68a 0x0005c910 0x0005a1b6 0x0005a196 0x0005a196 0x0005d724 
  sysworkq (pending): bottom: 200289a0, top: 200291a0, sp: 20029120
    0x00000bac: ??                           :?
    0x00050e16: z_swap_irqlock               /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/kernel/include/kswap.h:162
     (inlined by) z_swap                     /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/kernel/include/kswap.h:173
     (inlined by) z_pend_curr                /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/kernel/sched.c:709
    0x000525d4: z_sched_wait                 /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/kernel/sched.c:1682
    0x0004fe8c: work_queue_main              /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/kernel/work.c:627
    0x0005d724: z_thread_entry               /home/anan/.ubxlibpkg/nrfconnectsdk-v1.6.1/zephyr/lib/os/thread_entry.c:29
    Backtrace: 0x00050e16 0x000525d4 0x0004fe8c 0x0005d724 
  MPSL signal (pending): bottom: 2002c420, top: 2002c800, sp: 2002c790
...
```