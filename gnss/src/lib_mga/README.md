# Introduction
This directory contains a copy of the source code of `libMga`, a C library provided to u-blox GNSS customers to help with their integration of AssistNow, primarily on Linux/Windows systems.  It is taken from `libMga` `v22.07` (SHA 107c7e050df4367800a7fb0060292c225f93d7f8) and modified (so that it can be wrapped by [u_gnss_mga.c](../u_gnss_mga.c)) in at least the following respects:

- replace use of `double` with `int` types with a power of ten, so as not to bring in floating point libraries,
- `mgaBuildOnlineRequestParams()` and `mgaBuildOfflineRequestParams()` are updated to support a `NULL` buffer and perform length checking,
- additional fields are added [to the end] of the structure `MgaOnlineServerConfig`,
- the members of `MgaMsgInfo` are re-ordered to put the smallest at the end; most likely to lead to efficient structure packing,
- the `MgaFlowConfiguration` structure is modified so that the need to use the new `UBX-CFG-VAL` messages (rather than `UBX-CFG-NAVX5`) can be indicated,
- `assert()` becomes `U_ASSERT()`,
- `malloc()`/`free()` become `pUPortMalloc()`/`uPortFree()`,
- `time()` becomes `uPortGetTickTimeMs()`,
- `lock()` and `unlock()` use the `uPortMutexXxx()` API rather than native WIN32/Posix.
- `timezone` becomes `uPortGetTimezoneOffsetSeconds()`, which will work on both Linux and Windows,
- unused functions are `#if`'ed out, which also means that the Linux/Windows-specific headers can be `if`'ed out.
- renamed to match the `ubxlib` file naming convention and to namespace the files nicely.

Search for "MODIFIED" in the source code to find all of the modifications.