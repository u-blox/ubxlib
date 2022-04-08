**IMPORTANT: the SARAR5UCPU platform is currently only used internally within u-blox.  It is kept here in order that it remains in-sync with the development of `ubxlib` and is tested with `ubxlib` but it is NOT currently supported externally and the Git submodules it employs may NOT be visible.**

# Introduction
These directories provide the implementation of the porting layer on the SARAR5UCPU platform.

- `api`: contains the SARAR5UCPU platform specific apis.
- `src`: contains the implementation of the porting layers for SARAR5UCPU platform.
- `runner`: contains build files for SARAR5UCPU platform.
- `cfg`: contains the configuration for SARAR5UCPU platform.
- `u_cfg_os_platform_specific.h`: task priorities and stack sizes for the platform, built into this code.
