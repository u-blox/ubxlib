# Introduction
This folder contains some wrapper functions and one public function that perform maximum heap usage checking assuming that [newlib](https://sourceware.org/newlib/libc.html) provides the C library functions for a platform (which is often the case).

# Usage
To use these functions, as well as including them in the build, the following option must be passed to the GCC-compatible linker:

```
-Wl,--wrap=malloc -Wl,--wrap=_malloc_r -Wl,--wrap=calloc -Wl,--wrap=_calloc_r -Wl,--wrap=realloc -Wl,--wrap=_realloc_r
```

Note that the platform must provide a function `uPortInternalGetSbrkFreeBytes()`.  The way the heap works is that [newlib](https://sourceware.org/newlib/libc.html) will ask the ultimate heap owner, a function named `_sbrk()`, for memory as it requires.  So the heap size is the sum of the amount of free memory in [newlib](https://sourceware.org/newlib/libc.html) plus the amount of memory left in `_sbrk()`.  Hence `uPortInternalGetSbrkFreeBytes()` is called to determine what this is.