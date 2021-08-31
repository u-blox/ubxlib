# Introduction
This directory provides various utilities used by the other `ubxlib` components.

## [u_ringbuffer](api/u_ringbuffer.h)
A simple ring buffer implementation that functions as a wrapper for a linear buffer.
All API functions except `uRingBufferCreate()` and `uRingBufferDelete()` are thread-safe.

## [u_hex_bin_convert](api/u_hex_bin_convert.h)
Functions to convert a buffer of ASCII hex encoded into a buffer of binary and vice-versa.

## [u_time](api/u_time.h)
Functions to assist with time manipulation.
