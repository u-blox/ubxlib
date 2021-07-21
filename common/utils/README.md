# Introduction
This directory provides various utilities used by the other ubxlib components.

## u_ringbuffer
A simple ring buffer implementation that function as a wrapper for a linear buffer.
All API functions except uRingBufferCreate() and uRingBufferDelete() are thread-safe.
