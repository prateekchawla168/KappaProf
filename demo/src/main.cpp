
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
      PerfEvent monitor("hwgroup.csv");
      size_t time;
      drivee(monitor, time);
      auto report = monitor.GetReport(true);
      // monitor.PrintReport(report);
      DumpCSV(label + std::string("_hwdata.csv"), report);
      DumpTimeToCSV(label + std::string("_hwtime.csv"), i, time);
    }

    {
      PerfEvent monitor("cachegroup.csv");
      size_t time;
      drivee(monitor, time);
      auto report = monitor.GetReport(true);
      // monitor.PrintReport(report);
      DumpCSV(label + std::string("_cachedata.csv"), report);
      DumpTimeToCSV(label + std::string("_cachetime.csv"), i, time);
    }

    if (progress)
      std::cout << "Completed " << i + 1 << "/" << runs << "iterations. \r";
  }
}

void driver_dyn(driven_dynamic drivee, std::string label = "",
                int init_runs = 100, bool progress = true) {
  for (size_t j = 512; j <= init_runs; j += 512) {
    for (auto i = 0; i < 100; i++) {
      PerfEvent monitor("hwgroup.csv");
      size_t time;
      drivee(monitor, time, j);
      auto report = monitor.GetReport(true);
      // monitor.PrintReport(report);
      DumpCSV(label + std::string("_hwdata.csv"), report);
      DumpTimeToCSV(label + std::string("_hwtime.csv"), j, time);
    }

    if (progress)
      std::cout << "Completed " << j << "/" << init_runs << " iterations."
                << std::endl;
  }
}

int main(int argc, char* argv[]) {
  int runs = 4096;
  // driver(dgemm_kernel, "datafiles/dgemm", 10000, true);
  // driver(ddot_kernel, "datafiles/ddot", 10000, true);
  // driver(fftw_kernel, "datafiles/fftw", 10000, true);
  // driver(sum_kernel, "datafiles/sum", 10000, true);

  // driver_dyn(dynamic_sum_kernel, "datafiles/dyn_sum", runs, true);
  driver_dyn(dynamic_dgemm_kernel, "datafiles/dyn_dgemm", runs, true);

  return 0;
}
