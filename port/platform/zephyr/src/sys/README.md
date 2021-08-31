# Why This File Is Here
- The Zephyr integration of [newlib](https://sourceware.org/newlib/libc.html) is not thread-safe.
- Hence we have to use the minimal C library that comes with Zephyr instead.
- The minimal C library that comes with Zephyr does not include a definition for `struct timeval` in `time.h`.
- We could get around this by including `sys/_timeval.h`, which it does contain and has the definition, directly.
- However, some platforms put `_timeval.h` directly in their lib C include directory rather than in a `sys` sub-directory (e.g. the compiler we use for Linting does).
- We could work around this by making sure that the path to the `sys` sub-directory is in our list of includes and then just `#include "_timeval.h"`.
- However the GCC ARM compiler has a copy of `time.h` in the `sys` sub-directory *as well* as in the base C library include path and they are different.
- At that point we gave up and pasted a copy of [time.h](time.h) that met our needs into here.