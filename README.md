# CoPPer - Evaluation Utilities

This library is mostly glue, providing a framework to more easily evaluate CoPPer and compare against other controllers.
It wraps various other utilities and provides a common interface for initialization and teardown, configured through environment variables.
See [inc/copper-eval.h](inc/copper-eval.h) for environment variables and, when relevant, valid values.

Note that these utilities only work on UNIX-like systems.
Testing was performed on Linux: Ubuntu 14.04 (Trusty Tahr) and Ubuntu 16.04 (Xenial Xerus).

For details, please see the following and reference as appropriate:

* Connor Imes, Huazhe Zhang, Kevin Zhao, Henry Hoffmann. "Handing DVFS to Hardware: Using Power Capping to Control Software Performance". Technical Report [TR-2018-03](https://cs.uchicago.edu/research/publications/techreports/TR-2018-03). University of Chicago, Department of Computer Science. 2018.


## Prerequisities

This project uses `pkg-config` (through CMake) to locate and link with the following libraries; see each project for their transitive dependencies:

* [CoPPer](https://github.com/powercap/copper) - the primary controller being evaluated
* [RAPLCap](https://github.com/powercap/raplcap) - power capping actuation mechanism
* [heartbeats-simple-classic](https://github.com/libheartbeats/heartbeats-simple-classic) - instrumentation for performance and power behavior
* [POET](https://github.com/libpoet/poet) - an advanced DVFS controller
* [Linux cpufreq bindings](https://github.com/powercap/cpufreq-bindings) - DVFS actuation mechanism


## Building

This project uses CMake.

To build, run:

``` sh
mkdir _build
cd _build
cmake ..
make
```


## Installing

To install, run with proper privileges:

``` sh
make install
```

On Linux, installation typically places libraries in `/usr/local/lib` and header files in `/usr/local/include`.


## Uninstalling

Install must be run before uninstalling in order to have a manifest.
To uninstall, run with proper privileges:

``` sh
make uninstall
```


## Linking

Get linker information (including transitive dependencies) with `pkg-config`:

``` sh
pkg-config --libs --static copper-eval
```

Or in your Makefile, add to your linker flags with:

``` Makefile
$(shell pkg-config --libs --static copper-eval)
```

You may leave off the `--static` option if you built shared object libraries.

Depending on your install location, you may also need to augment your compiler flags with:

``` sh
pkg-config --cflags copper-eval
```


## Benchmarks

See the [benchmarks](benchmarks/) subdirectory for the benchmark patches used in CoPPer's evaluation.


## Project Source

Find this and related project sources at the [powercap organization on GitHub](https://github.com/powercap).  
This project originates at: https://github.com/powercap/copper-eval

Bug reports and pull requests are welcome.
