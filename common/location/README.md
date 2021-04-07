# Introduction
These directories provide a very simple location API, providing a means to establish position using any u-blox module, potentially in conjunction with a cloud service.  It relies on underlying APIs (`gnss`, `cell`, `wifi`, etc.) to do the heavy lifting.

# Usage
The directories include the API and the C source files necessary to call into the underlying `cell`, `wifi` and `gnss` APIs.  The `test` directory contains a small number of generic tests for the `location` API; for comprehensive tests of networking please refer to the test directory of the underlying APIs.

A simple usage example, obtaining position via a GNSS chip, is shown below.  Note that, before calling `app_start()` the platform must be initialised (clocks started, heap available, RTOS running), in other words `app_task()` can be thought of as a task entry point.  If you open the `u_main.c` file in the `app` directory of your platform you will see how we do this, with `main()` calling a porting API `uPortPlatformStart()` to sort that all out; you could paste the example code into `app_start()` there (and add the inclusions) as a quick and dirty test (`runner` will build it).

TODO: example