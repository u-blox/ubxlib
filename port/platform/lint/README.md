# Introduction
This directory provides stubs, configuration files and a Makefile which allows Lint to be run on the `ubxlib` code.

Since Lint requires tight integration with the compiler and C library files, and these are different for each platform, rather than performing the Lint integration N times, and to avoid the Lint filter being compromised by the need to accommodate coding choices made by each of the platforms, the files here allow the `ubxlib` platform independent code to be "built" under GCC on a PC so that Lint can analyze it.  Note that the platform code, the code under `port\platform\<vendor>`, is NOT checked.

The file `ubxlib.lnt` contains the Lint configuration options specific to `ubxlib`.  Also in this directory you will find the `*.txt` and `co-*.*` information and configuration files provided as a standard part of Flexelint.

Note that further `.lnt` configuration files, generated during the Linting process, will also be found here.

# Installation
To run Lint, as with any of the physical platforms, in addition to [Lint](https://gimpel.com/) itself some tools must be installed.  Note that we use Flexelint, rather than the newer PCLint, just because that's what we have a license for.

An installation of GCC for your PC is required. e.g.:

https://sourceforge.net/projects/tdm-gcc/

If this version of GCC is not on your path you may pass the location of `gcc.bin` and `g++.bin` to `make` on the command-line as follows:

`make GCC_BIN="C:\TDM-GCC-64\bin\gcc" GXX_BIN="C:\TDM-GCC-64\bin\g++"  -f co-gcc.mak`

Alternatively you may wish to install MinGw or Cygwin which will give you a C compiler and the Windows tools below (though obviously not `Unity`, you still need to clone that from Github) but is a much more "heavyweight" option.

The Make files provided with Flexelint require a few other tools which you may not find under Windows: `rm`, `touch` and `gawk`.  These must be installed and on the path.  They can be found in `unxutils` package which can be obtained from here:

https://sourceforge.net/projects/unxutils/

Also required is a version of Make, which must be on the path.  A version of Make is included in the above `unxutils` package BUT there are problems with it and so we use the version obtained from here (and hence this must be on the path before `unxutils`):

http://gnuwin32.sourceforge.net/packages/make.htm

In order that Lint can be run on the test code, you will also need a copy of Unity, the unit test framework, which can be Git cloned from here:

https://github.com/ThrowTheSwitch/Unity

Clone it to the same directory level as `ubxlib`, i.e.:

```
..
.
Unity
ubxlib
```

Note: you may put this repo in a different location but if you do so you will need to add, for instance, `UNITY_PATH=c:/Unity` on the command-line to `make`.

# Usage
Actually running Lint on your code is done through the automation scripts, for which you will need Python (e.g. 3.4) installed; see the `README.md` file in `port/platform/common/automation` for how to do this.  Note also the bit in the **Maintenance** section below about what to do if you are *adding* new code that you want Linted.

# Maintenance
- If you add a new `port` API, add a stub for it in `u_port_lint_stubs.c`.
- If you add a new #define to the `u_cfg_*.h` files in the `platform/<vendor>/<chipset>/cfg` directories, add it to the one in the `stubs` sub-directory here also; just a zero value will usually do.
- If you add a new directory that contains PLATFORM INDEPENDENT `.c` or `.cpp` files anywhere in the `ubxlib` tree, add it to the `LINT_DIRS` variable of the `port/platform/common/automation/u_run_lint.py` script.