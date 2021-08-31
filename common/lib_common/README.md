# `ubxlib` libraries

## General

Libraries in `ubxlib` sense simply consists of a header and some code. The header contains some metainfo on the library as such, like name, version, etc. It also contains a function table describing the functions and where they are placed. This latter part is the actual dynamic link information.

Libraries are position independent. They can be replaced during runtime if necessary. The code part can be encrypted.

## Lifetime of a library

The actual retrieval of the library blob is beyond this module. A library can for instance be downloaded from somewhere, or built and linked with the app - which pretty much voids the use of libraries altogether, but is good when developing libraries.

### `uLibProbe`

First thing to do before using a library is to probe it. Probing does not execute any library code but simply checks data structures. By probing the address where the library is supposed to be, you will get
* name
* version
* flags (library needs malloc, is encrypted, etc)

Or an error if there is no library.


### `uLibOpen`

Then, the library must be opened. This will populate the library handle used in further handling of the library. If the library has an initialiser, it will run now. The user can pass library specific flags if necessary. 

### `uLibGetCode` and `uLibRelocate`

If the probe indicates that the library is encrypted, the user needs to decrypt the code before calling the library. By calling `uLibGetCode` the user gets address and length of data to decrypt. After decrypting, the user indicates the decrypted area by calling `uLibRelocate`

Neither of these needs to be called if the library is in plaintext.

### `uLibSym`

The final step before using the library is to look up addresses of the library functions. Say the library provides following header api:
```C
int libFooAdd(int x, int y);
uint32_t libFooSum(const uint8_t *buf, uint32_t len);
```
Then, the following code would assign function pointers to looked up addresses:

```C
int (*libFooAdd)(int x, int y) = uLibSym(&libHdl, "libFooAdd");
uint32_t (*libFooSum)(const uint8_t *buf, uint32_t len) = uLibSym(&libHdl, "libFooSum");
```
And from this point the user can simply call:
```C
int res = libFooAdd(1,2);
uint32_t sum = libFooSum(someData, sizeof(someData));
```

### `uLibClose`

When the library is not needed anymore, it should be closed. If the library implements a finaliser, it will be called now. It is the library's responsibility to free anything allocated in the initialiser at this point.

### Sequence diagram, plaintext case

```
-----------          -----------          -----------
|   user  |          |  u_lib  |          |   lib   |
-----+-----          -----+-----          -----+-----
     |                    |                    |
     |                  get library blob       |
     |-------------------------------------------------->>
     |  uLibProbe(blob)   |                    |
     |------------------>>|                    |
     |         ok         |                    |
     |<<------------------|                    |
     |   uLibOpen(blob)   |                    |
     |------------------>>|                    |
     |                    | U_LIB_I_OPEN_FUNC  |
     |                    |------------------>>|
     |                    |         ok         |
     |                    |<<------------------|
     |      ok,handle     |                    |
     |<<------------------|                    |
     | uLibSym(h,"func1") |                    |
     |------------------>>|                    |
     |  address to func1  |                    |
     |<<------------------|                    |
     | uLibSym(h,"func2") |                    |
     |------------------>>|                    |
     |  address to func2  |                    |
     |<<------------------|                    |
     |                 func1(...)              |
     |--------------------------------------->>|
     |                 func2(...)              |
     |--------------------------------------->>|
     |    uLibClose(h)    |                    |
     |------------------>>|                    |
     |                    | U_LIB_I_CLOSE_FUNC |
     |                    |------------------>>|
     |                    |                    |
```
*Opening a library, calling it, and closing it*

### Sequence diagram, encrypted case

```
-----------          -----------          -----------
|   user  |          |  u_lib  |          |   lib   |
-----+-----          -----+-----          -----+-----
     |                    |                    |
     |                  get library blob       |
     |-------------------------------------------------->>
     |  uLibProbe(blob)   |                    |
     |------------------>>|                    |
     |         ok         |                    |
     |<<------------------|                    |
     |   uLibOpen(blob)   |                    |
     |------------------>>|                    |
     |                    | U_LIB_I_OPEN_FUNC  |
     |                    |------------------>>|
     |                    |         ok         |
     |                    |<<------------------|
     |      ok,handle     |                    |
     |<<------------------|                    |
     |   uLibGetCode(h)   |                    |
     |------------------>>|                    |
     | code address + len |                    |
     |<<------------------|                    |
     |           decrypt code to dst           |
     |-------------------------------------------------->>
     | uLibRelocate(h,dst)|                    |
     |------------------>>|                    |
     |         ok         |                    |
     |<<------------------|                    |
     | uLibSym(h,"func1") |                    |
     |------------------>>|                    |
     |  address to func1  |                    |
     |<<------------------|                    |
     | uLibSym(h,"func2") |                    |
     |------------------>>|                    |
     |  address to func2  |                    |
     |<<------------------|                    |
     |                 func1(...)              |
     |--------------------------------------->>|
     |                 func2(...)              |
     |--------------------------------------->>|
     |    uLibClose(h)    |                    |
     |------------------>>|                    |
     |                    | U_LIB_I_CLOSE_FUNC |
     |                    |------------------>>|
     |                    |                    |
```
*Opening a library, decrypt and relocate before calling*

# Writing a library
How to write a new library.

*There is an example library in [common/lib_common/example](example), libfibonacci, which is used as reference in following paragraphs.*

## Three golden rules

There are three things to think of when writing a new library.

**1. Make sure to include `u_lib_internal.h`**
```C
#include "u_lib_internal.h"
```

**2. Never use static variables or buffers.** Only use ram allocated on heap or stack.
```C
static uint32_t state; // NO!
static uint8_t buffer[16]; // NO!
static const char *msg = "Hello"; // OK, will end up in flash
int libFooBar(int x) {
    static uint32_t nogood = 0; // NO!
    ...
}

// OK, functions don't end up in .bss
static void fooUtil(int x, int y) {
    ...
}
```
Why? Because the libraries are only executed as code. There is no known OS, hence no proper dynamic linking, hence no dynamic `.bss` or `.data` sections

**3. No stdlib functions.** `malloc`, `memcpy`, `strlen`, `printf`, etc either needs to be reimplemented or passed as function pointers to the library. Nothing but your code is linked in the library, no external libraries are used. The makefile can possibly be yanked to include other static libraries though.

Why? Same reason as above: without dynamic linking from an OS, we need to do this by hand.

## Implement open and close if needed
Some libraries need to run an initialisation procedure upon opening. Other libraries must keep some kind of state, needing to malloc some memory for this.

`U_LIB_I_OPEN_FUNC` is called during opening:
```C
// prototype u_lib_internal.h:ulibOpenFn_t
int U_LIB_I_OPEN_FUNC(uLibLibc_t *pLibc, uint32_t flags, void **pCtx)
```

If a teardown is needed on close, implement `U_LIB_I_CLOSE_FUNC`:
```C
// prototype u_lib_internal.h:ulibCloseFn_t
void U_LIB_I_CLOSE_FUNC(void *ctx)
```

These functions are automatically detected as initialiser on open and finaliser on close when extracting symbols for the library header.

None of them are mandatory.

The `pLibc` argument to `U_LIB_I_OPEN_FUNC` is a struct containing function pointers to common libc functions which may be used by the library. If not needed, the pointers or the whole argument may be `NULL`.

If more libc functions are necessary for the library, the struct can be expanded in following manner. Say we have a library also needing `strcmp` and `memcpy`. Then, in the library header, we declare:
```C
// file lib_foo.h
#include "u_lib_internal.h"
...
typedef struct 
{
   uLibLibc_t uliblibc;
   int (*fnstrcmp)(const char *s1, const char *s2);
   int (*fnmemcpy)(void *dst, const void *src, uint32_t num);
 } uLibLibcExpanded_t;
 ...
```

This must be documented in the library, and handled in the open function:

```C
// file lib_foo.c
#include "lib_foo.h"
...
int U_LIB_I_OPEN_FUNC(uLibLibc_t *pLibc, uint32_t flags, void **pCtx)
{
  uLibLibcExpanded_t *pLibcx = (uLibLibcExpanded_t *)pLibc;
  ...
}
...
```
This way, the expanded struct can also be used by libraries using the classic `uLibLibc_t` struct.

Another way of passing extra functions to your library is to add an api call passing the needed functions, which must be called by user directly after opening the library.
For example:
```C
// file lib_foo.h
#include "u_lib_internal.h"
...
int libFooInitFunctions(
    void *ctx, 
    int (*pstrcmp)(const char *, const char *), 
    int (*pmemcpy)(void *, const void *, uint32_t)
);
...
```

*The example library [common/lib_common/example](example) keeps a state struct, which is malloced on open and freed on close.*

## Building the library

`lib_common` provides a generic makefile include script `common/lib_common/makelib.mk` which can be configured. This script
* compiles the library sources use `-fPIE` for position independent execution
* links the object files with `-shared` option
* objcopy `.text` and `.rodata` sections into a `code.binary file`
* optionally transforms the `code.binary` file, e.g. encrypt it
* objdump `.text` section for symbols into a symbol file
* parses the symbols file with a utility python script, generating library descriptor source code
* compiles and links the library descriptor, and objcopy to a `hdr.binary` file
* concatenates `hdr.binary` and `code.binary` into a library blob file
* also outputs the library blob as a C array, useful during library development

The prerequisites for building a library are:
* bash compatible prompt - `mv`, `cp`, `xxd`, `cat` etc
* gnumake
* python3
* compiler toolchain - `gcc` or `clang`, `objdump`, `objcopy`

Following environment variables are expected to be set:
* `CC` - the toolchain compiler
* `OBJDUMP` - the toolchain objdump
* `OBJCOPY` - the toolchain objcopy
* `PREFIX` - path where place build files, defaults to `.`
* `CFLAGS` - flags passed to compiler
* `CFILES` - library files to compile
* `INCLUDE` - directories to include, defaults to `.`
* `NAME` - name of the library, defaults to `undefined`
* `LIB_VERSION` - library version, defaults to `1`
* `LIB_FLAGS` - library flags, defaults to `0`
* `UBXLIB_PATH` - path to `ubxlib`, defaults to `../../`
* `TRANSFORM` - tool to transform library code (e.g. for encryption), defaults to `cp`

### Configure the build for your library

By creating a makefile including this script, it is fairly easy to compile your library. Here is an example of such a makefile for the example library fibonacci:
```make
# Define the fibonacci library
# Following parameters will be added to the library blob

# Library name
NAME = fibonacci
# Library version (uint32_t)
LIB_VERSION = 1
# Library flags (uint32_t, see lib_common/api/u_lib.h:U_LIB_HDR_FLAG_*)
LIB_FLAGS = 4 # this lib needs malloc

# Building parameters

# Path to ubxlib
UBXLIB_PATH = ../../../
# Library include directories
INCLUDE = . api
# Library source files
CFILES = src/lib_fibonacci.c

# call the library utility builder script
include $(UBXLIB_PATH)/common/lib_common/makelib.mk
```

`makelib.mk` uses ordinary environment variables such as `CC`, `PREFIX`, `CFLAGS`, etc which makes it reconfigurable for different build systems (e.g. CMake) and cross-compiling. 

*If you try running `make` in the example library directory [common/lib_common/example](example), the default environment will be used. This will compile the library for your current platform, probably x86-64, given you have a build environment.*