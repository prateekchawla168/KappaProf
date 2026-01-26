#pragma once

#include <fstream>
#include <iostream>
#include <string>

#include "profiler.hpp"

void DumpCSV(std::string filename, std::vector<EventType>& report) {
  std::ofstream csvfile;
  csvfile.open(filename, std::ios::app);

  if (!csvfile) {
    std::cerr << "Error opening file " << filename << std::endl;
    return;
  }

  // if file is empty then create a header
  csvfile.seekp(0, std::ios::end);
  bool is_empty = (csvfile.tellp() == 0);
  if (is_empty) {
    std::string header;
    for (auto i = 0; i < report.size(); ++i) {
      header.append(report[i].GetName());
      if (i < (report.size() - 1))
        header.append(",");  // last item should not have the comma
    }
    csvfile << header << "\n";
  }
  // dump data
  std::string data;
  for (auto i = 0; i < report.size(); ++i) {
    data.append(std::to_string(report[i].GetCount()));
    if (i < (report.size() - 1)) data.append(",");
  }
  csvfile << data << "\n";
  csvfile.close();
}

void DumpTimeToCSV(std::string filename, int iteration, long long time) {
  std::ofstream csvfile;
  csvfile.open(filename, std::ios::app);

  if (!csvfile) {
    std::cerr << "Error opening file " << filename << std::endl;
    return;
  }

  csvfile.seekp(0, std::ios::end);
  bool is_empty = (csvfile.tellp() == 0);
  if (is_empty) csvfile << "runID,time";

  std::string data;
  data.append(std::to_string(iteration));
  data.append(",");
  data.append(std::to_string(time));

  csvfile << data << std::endl;

  csvfile.close();
}