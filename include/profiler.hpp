#pragma once

#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class EventType {
 private:
  std::pair<std::string, long double> item;

 public:
  /// we need to copy the name!
  EventType(std::string _name, long double _count) {
    item.first = _name;
    item.second = _count;
  }

  EventType() {
    item.first = "";
    item.second = -1;
  }

  std::string GetName() { return item.first; }

  long double GetCount() { return item.second; }

  void SetName(std::string _name) { item.first = _name; }

  void SetCount(long double _count) { item.second = _count; }

  size_t GetSize() { return sizeof(item); }
};

class PerfEvent {
 public:
  struct Event {
    // since we multiplex events, we need to correct for this.
    // simplest method is to normalize wrt timeEnabled/timeElapsed
    struct EventDataFormat {
      uint64_t value;
      uint64_t timeEnabled;
      uint64_t timeElapsed;
      // uint64_t id;
    } prev, data;

    perf_event_attr pe;
    int fd;
    int leaderFD;
    bool isLeader;

    long double readCounter() {
      // long double multiplexingCorrection =
      //     static_cast<double>(data.timeEnabled - prev.timeEnabled) /
      //     (data.timeElapsed - prev.timeElapsed);
      // if ((data.timeElapsed - prev.timeElapsed) == 0)
      //   multiplexingCorrection = 1.0;
      // return static_cast<long double>(data.value - prev.value) *
      //        multiplexingCorrection;
      return static_cast<long double>(data.value);
    }

    // Need a slightly better way to do this
    bool operator==(const Event& other) const {
      return pe.config == other.pe.config;
    }
  };

  // perf_event_open(2) for details
  enum EventDomain : uint8_t {
    USER = 0b1,
    KERNEL = 0b10,
    HYPERVISOR = 0b100,
    ALL = 0b111
  };

  void RegisterCounter(const std::string&, int&, uint64_t, uint64_t,
                       EventDomain);

  void StartCounters();

  void StopCounters();

  void ReadCounterList(const std::string&);

  std::vector<std::string> GetCounterNames() { return names; }

  long double GetCounter(const std::string&);

  long double GetDuration() {
    // returns nanoseconds by default
    return std::chrono::duration_cast<std::chrono::nanoseconds>(stopTime -
                                                                startTime)
        .count();
  }

  long double GetIPC() {
    return (long double)GetCounter("instructions") /
           (long double)GetCounter("cycles");
  }

  long double GetCPUs() {
    return (long double)GetCounter("task-clock") /
           (long double)(GetDuration() * 1e9);
  }

  long double GetAvgGHz() {
    return (long double)GetCounter("cycles") /
           (long double)GetCounter("task-clock");
  }

  std::vector<EventType> GetReport(long double, bool);
  std::vector<EventType> GetReport() { return this->GetReport(1.0, false); };
  std::vector<EventType> GetOverhead();
  void PrintReport();
  void PrintReport(std::vector<EventType>);

  PerfEvent();
  PerfEvent(const std::string&);
  ~PerfEvent();

 private:
  void ConstructTypeMap();
  int TypeLookup(const std::string&);

 private:
  std::vector<Event> events;
  std::vector<std::string> names;
  std::unordered_map<std::string, int> typeMap;
  std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
  std::chrono::time_point<std::chrono::high_resolution_clock> stopTime;
};