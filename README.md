# κProf: A lightweight, fast kernel benchmarking library

## 0. Prerequisites

- A C++20-compatible compiler
- CMake
- Linux operating system
- *Perf* must be installed in the OS. (This is included by default in most cases.) 

## 1. Building

κProf uses the CMake build system. It is recommended to perform an out of tree build. It is assumed that the current directory is the source directory.

```sh
$ pwd
/path/to/kProf/

$ cmake -B build -S . 
$ cmake --build build
```

In case one wants to install this library, the following command must be run after the previous steps.

```sh
$ cmake --install build
```

An optional demo is included in this code. In order to compile and run it, use the flag `-DBUILD_DEMO=ON` during building the code.  

## 2. Usage

To include κProf in your source code, include the header `kprof.hpp`. This provides all the functionality in the `kProf` namespace.
- To initialize a counter, use `kProf::KProfEvent <objectName>`. 
- When instrumenting the code, use `<objectName>.StartCounters()` and `<objectName>.StopCounters()`.
- Access and print reports with `<objectName>.GetReport(true)` and `<objectName>.PrintReport()`.  In case raw event counts are required (without κProf removing its overhead), pass `false` to `<objectName>.GetReport()`. To print a specific report, pass it as an argument to `<objectName>.PrintReport()`. 
- In case the code is single-threaded, it is recommended to pin the resulting executable to a single core. `numactl` is recommended.
- Counter information can be provided at runtime by:
  - Set the environment variable `KPROF_COUNTER_FILE` to a CSV file containing the counter information.
    - The file formatting must be: 
    ```CSV
        label,counter_type,counter_spec
    ```
    For example, `HW-instructions,PERF_TYPE_HARDWARE,PERF_COUNT_HW_INSTRUCTIONS` is a valid line. Consult the manpage for `perf_event_open` for possible values.
  - Set the environment variable `KPROF_COUNTER_CONF` to valid set of counters.     
    - The counters must be formatted as `label,type:val;` for each counter, delimited by `;`.
    - Example: `KPROF_COUNTER_CONF=LLC-misses-intel,R:0x2E41` for counting LLC misses on Intel architectures. 
    - Hex codes must begin with `0x` or `0X`.
    - Counter types must be `H`, `S`, `C`, or `R` for Hardware, software, cache, and raw pointers. Hex codes or decimals for event IDs must always be specified with type `R`. 
  - Provide a `std::string` argument pointing the CSV file when initializing the `kProf::KProfEvent` object.


It is also possible to use `kPRof` as a dependency in your CMake project. Your executable/library needs to be linked to `kProf`.