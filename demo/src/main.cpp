#include <cstdio>
#include <iostream>
#include <string>

#include "csvhelper.hpp"
#include "kernel.hpp"
#include "profiler.hpp"

typedef void (*driven)(PerfEvent&, size_t&);

typedef void (*driven_dynamic)(PerfEvent&, size_t&, size_t);

void driver(driven drivee, std::string label = "", int runs = 100,
            bool progress = true) {
  {
    // discard the first run as warmup
    PerfEvent monitor;
    size_t time;
    drivee(monitor, time);
  }

  for (auto i = 0; i < runs; i++) {
    {
      PerfEvent monitor(label);
      size_t time;
      drivee(monitor, time);
      auto report = monitor.GetReport(true);
      // monitor.PrintReport(report);
      DumpCSV(label + "_hwdata.csv", report);
      DumpTimeToCSV(label + "_hwtime.csv", i, time);
    }

    {
      PerfEvent monitor("cachegroup.csv");
      size_t time;
      drivee(monitor, time);
      auto report = monitor.GetReport(true);
      // monitor.PrintReport(report);
      DumpCSV(label + "_cachedata.csv", report);
      DumpTimeToCSV(label + "_cachetime.csv", i, time);
    }

    if (progress)
      std::cout << "Completed " << i + 1 << "/" << runs << "iterations. \r";
  }
}

void driver_dyn(driven_dynamic drivee, std::string label = "",
                int init_runs = 100, bool progress = true) {
  for (size_t j = 1; j <= init_runs; ++j) {
    for (auto i = 0; i < 100; i++) {
      PerfEvent monitor("cachegroup.csv");
      size_t time;
      drivee(monitor, time, j);
      auto report = monitor.GetReport(true);
      // monitor.PrintReport(report);
      DumpCSV(label + "_cachedata.csv", report);
      DumpTimeToCSV(label + "_cachetime.csv", j, time);
    }

    if (progress)
      std::cout << "Completed " << j << "/" << init_runs << "iterations. \r";
  }
}

int main(int argc, char* argv[]) {
  int runs = 10;
  // driver(dgemm_kernel, "datafiles/dgemm", runs, true);
  // driver(ddot_kernel, "datafiles/ddot", runs, true);
  // driver(fftw_kernel, "datafiles/fftw", runs, true);
  // driver(sum_kernel, "datafiles/sum", runs, true);

  driver_dyn(dynamic_sum_kernel, "datafiles/dyn_sum", runs, true);

  return 0;
}
