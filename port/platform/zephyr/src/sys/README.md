# Why This File Is Here
- By default, Zephyr uses its own minimal C library.
- This minimal C library does not include a definition for `struct timeval` in `time.h`.
- We could get around this by including `sys/_timeval.h`, which it does contain and has the definition, directly.
- However, some platforms put `_timeval.h` directly in their lib C include directory rather than in a `sys` sub-directory (e.g. the compiler we use for Linting does).
- We could work around this by making sure that the path to the `sys` sub-directory is in our list of includes and then just `#include "_timeval.h"`.
- However the GCC ARM compiler has a copy of `time.h` in the `sys` sub-directory *as well* as in the base C library include path and they are different.
- At that point we gave up and pasted a copy of [time.h](time.h) that met our needs into here.

None of the above applies when compiling Zephyr for Linux/Posix, which uses `newlib` instead of the Zephyr minimal C library, however there is no way to make this include file disappear for the Linux/Posix case and so we're stuck with using this header file for the `newlib` case also.