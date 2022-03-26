# Why This File Is Here
- Arm compiler does not include a definition for `struct timeval` in `time.h`.
- We could get around this by including definition of `struct timeval` in `time.h` here.
- Some platforms put `_timeval.h`, which contains definition of `timeval`, directly in their lib C include directory rather than in a `sys` sub-directory (e.g. the compiler we use for Linting does).
- We could work around this by making sure that the path to the `sys` sub-directory is in our list of includes and then just `#include "time.h"`.