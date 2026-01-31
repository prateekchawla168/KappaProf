
#include <cstdio>
#include <iostream>
#include <string>

#include "csvhelper.hpp"
#include "kernel.hpp"
#include "kprof.hpp"

using namespace KProf;

typedef void (*driven)(KProfEvent&, size_t&);

typedef void (*driven_dynamic)(KProfEvent&, size_t&, size_t);

void driver(driven drivee, std::string label = "", int runs = 100,
            bool progress = true) {
  {
    // discard the first run as warmup
    KProfEvent monitor;
    size_t time;
    drivee(monitor, time);
  }

  for (auto i = 0; i < runs; i++) {
    {
      KProfEvent monitor("hwgroup.csv");
      size_t time;
      drivee(monitor, time);
      auto report = monitor.GetReport(true);
      // monitor.PrintReport(report);
      DumpCSV(label + std::string("_hwdata.csv"), report);
      DumpTimeToCSV(label + std::string("_hwtime.csv"), i, time);
    }

    {
      KProfEvent monitor("cachegroup.csv");
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
      KProfEvent monitor("hwgroup.csv");
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
  driver(dgemm_kernel, "datafiles/del_dgemm", 10000, true);
  driver(ddot_kernel, "datafiles/del_ddot", 10000, true);
  driver(fftw_kernel, "datafiles/del_fftw", 10000, true);
  driver(sum_kernel, "datafiles/del_sum", 10000, true);

  // driver_dyn(dynamic_sum_kernel, "datafiles/dyn_sum", runs, true);
  // driver_dyn(dynamic_dgemm_kernel, "datafiles/dyn_dgemm", runs, true);

  return 0;
}
