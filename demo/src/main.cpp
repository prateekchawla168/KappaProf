#include <cstdio>
#include <iostream>
#include <string>

#include "csvhelper.hpp"
#include "kernel.hpp"
#include "profiler.hpp"

int main(int argc, char* argv[]) {
  {
    // discard the first run as warmup
    // PerfEvent monitor;
    // kernel(monitor);
    long long time;
    kernel(time);
  }

  for (auto i = 0; i < 10000; i++) {
    // }
    //   PerfEvent monitor("hwgroup.csv");
    //   kernel(monitor);
    //   auto report = monitor.GetReport(1.0, true);
    //   // monitor.PrintReport(report);
    //   DumpCSV("hw_data_2.csv", report);
    // }

    // {
    //   PerfEvent monitor("cachegroup.csv");
    //   kernel(monitor);
    //   auto report = monitor.GetReport(1.0, true);
    //   // monitor.PrintReport(report);
    //   DumpCSV("cache_data_2.csv", report);
    // }

    // {
    //   PerfEvent monitor("swgroup.csv");
    //   kernel(monitor);
    //   auto report = monitor.GetReport(1.0, true);
    //   // monitor.PrintReport(report);
    //   DumpCSV("sw_data_2.csv", report);
    // }

    long long time;
    kernel(time);

    DumpTimeToCSV("Rawtime.csv", i, time);

    std::cout << "Completed " << i + 1 << "/1000 iterations. \r";
  }
  return 0;
}
