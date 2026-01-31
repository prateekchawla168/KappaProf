#pragma once

#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace KProf {
class KProfCounter {
 private:
  std::pair<std::string, uint64_t> item;

 public:
  /// we need to copy the name!
  KProfCounter(std::string _name, uint64_t _count) {
    item.first = _name;
    item.second = _count;
  }

  KProfCounter() {
    item.first = "";
    item.second = -1;
  }

  std::string GetName() { return item.first; }

  long long GetCount() { return item.second; }

  void SetName(std::string _name) { item.first = _name; }

  void SetCount(uint64_t _count) { item.second = _count; }

  size_t GetSize() { return sizeof(item); }
};

struct ReadFormat {
  uint64_t nr;
  struct {
    uint64_t value;
    uint64_t id;
  } values[10];
};

class KProfEvent {
 public:
  struct Event {
    struct EventDataFormat {
      uint64_t value;
    } data;

    perf_event_attr pe;
    int fd;
    int leaderFD;
    bool isLeader;
    int numCounters;
    uint64_t id;

    uint64_t readCounter() { return data.value; }
    inline void SetData(uint64_t v) { data.value = v; }

    Event() {
      fd = -1;
      leaderFD = -1;
      isLeader = false;
      numCounters = 0;
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

  std::vector<std::string> GetCounterNames() { return names; }

  uint64_t GetCounter(const std::string&);

  uint64_t GetDuration() {
    // returns nanoseconds by default
    return std::chrono::duration_cast<std::chrono::nanoseconds>(stopTime -
                                                                startTime)
        .count();
  }

  std::vector<KProfCounter> GetReport(bool);  // todo: de-idiotify
  std::vector<KProfCounter> GetReport() { return this->GetReport(false); };
  void PrintReport();
  void PrintReport(std::vector<KProfCounter>);

  KProfEvent();
  KProfEvent(const std::string&);
  ~KProfEvent();

 private:
  void ConstructTypeMap();
  int TypeLookup(const std::string&);
  std::vector<KProfCounter> GetOverhead();
  void ReadCounterList(const std::string&);
  void ReadEnvConfig(bool, bool&, std::string&);
  void ParseEnvConfig(std::string&);

 private:
  std::vector<Event> events;
  std::vector<std::string> names;
  std::vector<int> leaderFDs;
  std::unordered_map<std::string, int> typeMap;
  std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
  std::chrono::time_point<std::chrono::high_resolution_clock> stopTime;
};

};  // namespace KProf
