# Introduction
This component defines a `U_ASSERT()` macro (in [u_assert.h](api/u_assert.h)) which is called throughout `ubxlib` where the C-library `assert()` function might normally be called.

# Usage
If the condition passed to `U_ASSERT()` evaluates to `false` then `uAssertFailed()` is called with the file name and line number of `U_ASSERT()`; `uAssertFailed()` simply prints out this file and line number information and then enters an infinite loop.

`uAssertFailed()` is defined to have weak linkage, so you can override it by defining your own implementation of the `uAssertFailed()` function.  Alternatively you may call `uAssertHookSet()` to register an assert failure function which will override the default behaviour of `uAssertFailed()`.  In both cases it is up to your function to take appropriate action and **not** return; should your function return then code execution will just continue from after the failed `U_ASSERT()` macro.

If the conditional compilation flag `U_CFG_DISABLE_ASSERT` is defined then no `U_ASSERT()` macros will have any effect.  This is quite a dangerous thing to do; the `U_ASSERT()` function is called when an unrecoverable programming error has been detected, removing such checks leaves the code unprotected.