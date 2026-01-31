#include "profiler.hpp"

#include <algorithm>  // for std::find
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "ErrorHandler.hpp"

// clang-format off
#define CACHE_MISS_R    ((PERF_COUNT_HW_CACHE_OP_READ  << 8)    | (PERF_COUNT_HW_CACHE_RESULT_MISS   << 16))
#define CACHE_MISS_W    ((PERF_COUNT_HW_CACHE_OP_WRITE << 8)    | (PERF_COUNT_HW_CACHE_RESULT_MISS   << 16))
#define CACHE_ACCESS_R  ((PERF_COUNT_HW_CACHE_OP_READ  << 8)    | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))
#define CACHE_ACCESS_W  ((PERF_COUNT_HW_CACHE_OP_WRITE << 8)    | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))
#define CACHE_MISS_P    ((PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS   << 16))
#define CACHE_ACCESS_P  ((PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))
// clang-format on
namespace KProf {

void KProfEvent::RegisterCounter(const std::string& name, int& leader_FD,
                                 uint64_t type, uint64_t eventID,
                                 EventDomain domain = ALL) {
  auto event = Event();
  auto& pe = event.pe;
  memset(&pe, 0, sizeof(struct perf_event_attr));
  pe.type = static_cast<uint32_t>(type);
  pe.size = sizeof(struct perf_event_attr);
  pe.config = eventID;
  pe.disabled = 1;
  pe.inherit = 1;
  pe.inherit_stat = 0;
  pe.pinned = 0;
  pe.exclude_user = !(domain & USER);
  pe.exclude_kernel = !(domain & KERNEL);
  pe.exclude_hv = !(domain & HYPERVISOR);
  pe.read_format = PERF_FORMAT_ID | PERF_FORMAT_GROUP;

  event.isLeader = (leader_FD == -1) ? true : false;

  bool secondCallWasNeeded = false;

  event.fd = static_cast<int>(
      syscall(SYS_perf_event_open, &event.pe, 0, -1, leader_FD, 0));
  if (event.fd < 0) {
    if ((errno == 22) || (errno == ENOSPC)) {
      std::cerr << "Could not open " << name
                << " with the specified leader. "
                   "Re-attempting as leader."
                << std::endl;
      event.fd = static_cast<int>(
          syscall(SYS_perf_event_open, &event.pe, 0, -1, -1, 0));
      secondCallWasNeeded = true;
      event.isLeader = true;
    }
    if (event.fd < 0) {  // need to check a second time
      std::cerr << "Error " << errno << " opening counter " << name << ": "
                << DescribeError(errno)
                << ". Ignoring this counter on the current system. "
                << std::endl;
    }
  } else {
    if (event.isLeader) {
      event.leaderFD = event.fd;
      leader_FD = event.fd;  // send this to the next call to RegisterCounter
      // set the id of this thing to whatever we need, and set contained objects
      // to 1
      event.numCounters = 1;
      leaderFDs.push_back(event.fd);

    } else {
      event.leaderFD = leader_FD;
      // this is not the leader -> find it!
      for (auto i = 0; i < events.size(); ++i) {
        if (events[i].fd == leader_FD) {
          events[i].numCounters++;
          break;
        }
      }
    }
    // we managed to get the event, so syscall for its id
    auto ret = ioctl(event.fd, PERF_EVENT_IOC_ID, &event.id);
    if (ret == -1) {
      std::stringstream errmsg;
      errmsg << "PERF_EVENT_IOC_ID failed for " << name << "! : " << errno
             << " " << DescribeError_IOCTL(errno);
      throw std::runtime_error(errmsg.str());
    }

    events.push_back(event);
    names.push_back(name);
    // event was successfully added
  }
}

void KProfEvent::StartCounters() {
  // could use a std::for_each but we need the index.
  // TODO: Define an enumerate()?
  // for (size_t i = 0; i < events.size(); i++) {
  //   auto& event = events[i];
  //   if (!event.isLeader) continue;
  //   auto& name = names[i];
  //   auto ret = ioctl(event.fd, PERF_EVENT_IOC_RESET, 0);
  //   if (ret == -1) {
  //     std::stringstream errmsg;
  //     errmsg << "PERF_EVENT_IOC_RESET failed for " << name << "! : " << errno
  //            << " " << DescribeError_IOCTL(errno);
  //     throw std::runtime_error(errmsg.str());
  //   }
  //   startTime = std::chrono::high_resolution_clock::now();
  //   ret = ioctl(event.fd, PERF_EVENT_IOC_ENABLE, 0);
  //   if (ret == -1) {
  //     std::stringstream errmsg;
  //     errmsg << "PERF_EVENT_IOC_ENABLE failed for " << name << "! : " <<
  //     errno
  //            << " " << DescribeError_IOCTL(errno);
  //     throw std::runtime_error(errmsg.str());
  //   }
  // }

  for (auto& fd : leaderFDs) {
    auto ret = ioctl(fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    if (ret == -1) {
      std::stringstream errmsg;
      errmsg << "PERF_EVENT_IOC_RESET failed for " << fd << "! : " << errno
             << " " << DescribeError_IOCTL(errno);
      throw std::runtime_error(errmsg.str());
    }
    startTime = std::chrono::high_resolution_clock::now();
    ret = ioctl(fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    if (ret == -1) {
      std::stringstream errmsg;
      errmsg << "PERF_EVENT_IOC_ENABLE failed for " << fd << "! : " << errno
             << " " << DescribeError_IOCTL(errno);
      throw std::runtime_error(errmsg.str());
    }
  }
}

void print_readfmt(ReadFormat fmt) {
  std::cout << "tr:" << std::endl;
  std::cout << "nr: " << fmt.nr << std::endl;
  std::cout << "values:" << std::endl;
  for (auto i = 0; i < fmt.nr; i++) {
    std::cout << "\tid: " << fmt.values[i].id << std::endl;
    std::cout << "\tvalue: " << fmt.values[i].value << std::endl;
  }
}

void KProfEvent::StopCounters() {
  // for (unsigned i = 0; i < events.size(); ++i) {
  //   // auto& event = events[i];
  //   if (!events[i].isLeader) continue;
  //   // this is a leader -> Stop this and the whole group gets stopped
  //   auto ret = ioctl(events[i].fd, PERF_EVENT_IOC_DISABLE,
  //   PERF_IOC_FLAG_GROUP); if (ret == -1) {
  //     std::stringstream errmsg;
  //     errmsg << "PERF_EVENT_IOC_DISABLE failed for " << names[i]
  //            << "! : " << errno << " " << DescribeError_IOCTL(errno);
  //     throw std::runtime_error(errmsg.str());
  //   }
  //   stopTime = std::chrono::high_resolution_clock::now();

  //   // temp structure allocation to read things
  //   ReadFormat tmp;
  //   ret = read(events[i].fd, &tmp, sizeof(tmp));
  //   if (ret < 0) {
  //     std::stringstream errmsg;
  //     errmsg << "Read() error: " << errno << ": " << strerror(errno)
  //            << std::endl;
  //     throw std::runtime_error(errmsg.str());
  //   }
  //   tmp.print();

  //   // we read the counters, now assign them
  //   for (int j = 0; j < events.size(); j++) {
  //     if (events[j].leaderFD == events[i].fd) {
  //       // this event is from this group -> find it and allocate
  //       // the counter with the correct id
  //       for (int k = 0; k < tmp.nr; k++) {
  //         if (tmp.values[k].id == events[j].id) {
  //           // set up the event
  //           events[j].SetData(tmp.values[k].value, tmp.te, tmp.tr);
  //           // we're done, exit
  //           break;
  //         }
  //       }
  //     }
  //   }
  // }

  for (auto& fd : leaderFDs) {
    auto ret = ioctl(fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
    if (ret == -1) {
      std::stringstream errmsg;
      errmsg << "PERF_EVENT_IOC_DISABLE failed for " << fd << "! : " << errno
             << " " << DescribeError_IOCTL(errno);
      throw std::runtime_error(errmsg.str());
    }
    stopTime = std::chrono::high_resolution_clock::now();

    ReadFormat tmp;
    ret = read(fd, &tmp, sizeof(tmp));
    if (ret < 0) {
      std::stringstream errmsg;
      errmsg << "Read() error: " << errno << ": " << strerror(errno)
             << std::endl;
      throw std::runtime_error(errmsg.str());
    }
    // print_readfmt(tmp);

    for (int j = 0; j < events.size(); j++) {
      if (events[j].leaderFD == fd) {
        // this event is from this group -> find it and allocate
        // the counter with the correct id
        for (int k = 0; k < tmp.nr; k++) {
          if (tmp.values[k].id == events[j].id) {
            // set up the event
            events[j].SetData(tmp.values[k].value);
            // we're done, exit
            break;
          }
        }
      }
    }
  }
}

uint64_t KProfEvent::GetCounter(const std::string& name) {
  for (size_t i = 0; i < events.size(); i++)
    if (names[i] == name) return events[i].readCounter();
  return -1;
}

std::vector<KProfCounter> KProfEvent::GetReport(
    bool overheadCorrection = false) {
  std::vector<KProfCounter> report(names.size() + 1);
  for (size_t i = 0; i < report.size() - 1; ++i) {
    std::string name = names[i];
    auto count = GetCounter(names[i]);
    report[i].SetName(name);
    report[i].SetCount(count);
  }
  report[names.size()].SetName("Wall-time");
  report[names.size()].SetCount(GetDuration());

  if (overheadCorrection) {
    auto overhead = GetOverhead();
    for (size_t i = 0; i < report.size(); ++i)
      report[i].SetCount(report[i].GetCount() - overhead[i].GetCount());
  }
  return report;
}

void KProfEvent::PrintReport() {
  for (size_t i = 0; i < names.size(); ++i) {
    std::cout << std::format("{} : {}", names[i],
                             (long long)GetCounter(names[i]))
              << std::endl;
  }
  return;
}

void KProfEvent::PrintReport(std::vector<KProfCounter> report) {
  for (size_t i = 0; i < report.size(); ++i) {
    std::cout << std::format("{} : {}", report[i].GetName(),
                             (long long)report[i].GetCount())
              << std::endl;
  }
  return;
}

KProfEvent::KProfEvent() {
  // first, check the environment config

  std::string parsedEnvString;
  bool found;
  ReadEnvConfig(true, found, parsedEnvString);
  switch (found) {
    case true:
      // we found a config file from the environment
      ReadCounterList(parsedEnvString);
      break;
    case false:
      // no env file, check for a list in the environment
      ReadEnvConfig(false, found, parsedEnvString);
      if (found) ParseEnvConfig(parsedEnvString);
      break;
  }
  if (found) return;  // we did things we were meant to, leave.

  // i can't find anything - default set should be initialized

  // prevent formatter from messing with this
  // clang-format off

  int dummy = -1;
  RegisterCounter("HW-instructions"   , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS                , USER);
  dummy = -1;
  RegisterCounter("CPU-cycles"        , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES                  , USER); 
  dummy = -1;
  RegisterCounter("Branch-instuctions", dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS         , USER); 
  dummy = -1;
  RegisterCounter("Branch-misses"     , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES               , USER); 
  dummy = -1;
  RegisterCounter("Bus-cycles"        , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES                  , USER); 
  dummy = -1;
  RegisterCounter("Stall-frontend"    , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND     , USER); 
  dummy = -1;
  RegisterCounter("Stall-backend"     , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND      , USER); 
  dummy = -1;
  RegisterCounter("L1d-read-miss"     , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_MISS_R    , USER); 
  dummy = -1;
  RegisterCounter("L1d-write-miss"    , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_MISS_W    , USER); 
  dummy = -1;
  RegisterCounter("L1d-read-access"   , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_ACCESS_R  , USER); 
  dummy = -1;
  RegisterCounter("L1d-write-access"  , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_ACCESS_W  , USER); 
  dummy = -1;
  RegisterCounter("L1i-read-miss"     , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_MISS_R    , USER); 
  dummy = -1;
  RegisterCounter("L1i-write-miss"    , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_MISS_W    , USER); 
  dummy = -1;
  RegisterCounter("L1i-read-access"   , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_ACCESS_R  , USER); 
  dummy = -1;
  RegisterCounter("L1i-write-access"  , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_ACCESS_W  , USER); 
  dummy = -1;
  RegisterCounter("LLC-read-miss"     , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_MISS_R    , USER); 
  dummy = -1;
  RegisterCounter("LLC-write-miss"    , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_MISS_W    , USER); 
  dummy = -1;
  RegisterCounter("LLC-read-access"   , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_ACCESS_R  , USER); 
  dummy = -1;
  RegisterCounter("LLC-write-access"  , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_ACCESS_W  , USER); 
  dummy = -1;
  RegisterCounter("Pagefaults-total"  , dummy, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS                 , USER); 
  dummy = -1;
  RegisterCounter("Pagefaults-maj"    , dummy, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ             , USER); 
  dummy = -1;
  RegisterCounter("Pagefaults-min"    , dummy, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN             , USER); 
  dummy = -1;
  RegisterCounter("Alignment-faults"  , dummy, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS            , USER); 
  dummy = -1;
  RegisterCounter("CPU-migrations"    , dummy, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS              , USER);

  // clang-format on

  return;
}

KProfEvent::KProfEvent(const std::string& configFile) {
  if (typeMap.empty()) ConstructTypeMap();
  ReadCounterList(configFile);
}

KProfEvent::~KProfEvent() {
  for (auto& event : events) {
    close(event.fd);
  }
}

std::vector<KProfCounter> KProfEvent::GetOverhead() {
  StartCounters();
  StopCounters();
  return GetReport(false);
}

void KProfEvent::ConstructTypeMap() {
  // clang-format off
  typeMap["PERF_TYPE_HARDWARE"] = PERF_TYPE_HARDWARE;
  typeMap["PERF_TYPE_SOFTWARE"] = PERF_TYPE_SOFTWARE;
  typeMap["PERF_TYPE_HW_CACHE"] = PERF_TYPE_HW_CACHE;
  typeMap["PERF_TYPE_RAW"] = PERF_TYPE_RAW;
  
  typeMap["PERF_COUNT_HW_CPU_CYCLES"] = PERF_COUNT_HW_CPU_CYCLES;
  typeMap["PERF_COUNT_HW_INSTRUCTIONS"] = PERF_COUNT_HW_INSTRUCTIONS;
  typeMap["PERF_COUNT_HW_CACHE_REFERENCES"] = PERF_COUNT_HW_CACHE_REFERENCES;
  typeMap["PERF_COUNT_HW_CACHE_MISSES"] = PERF_COUNT_HW_CACHE_MISSES;
  typeMap["PERF_COUNT_HW_BRANCH_INSTRUCTIONS"] = PERF_COUNT_HW_BRANCH_INSTRUCTIONS;
  typeMap["PERF_COUNT_HW_BRANCH_MISSES"] = PERF_COUNT_HW_BRANCH_MISSES;
  typeMap["PERF_COUNT_HW_BUS_CYCLES"] = PERF_COUNT_HW_BUS_CYCLES;
  typeMap["PERF_COUNT_HW_STALLED_CYCLES_FRONTEND"] = PERF_COUNT_HW_STALLED_CYCLES_FRONTEND;
  typeMap["PERF_COUNT_HW_STALLED_CYCLES_BACKEND"] = PERF_COUNT_HW_STALLED_CYCLES_BACKEND;
  typeMap["PERF_COUNT_HW_REF_CPU_CYCLES"] = PERF_COUNT_HW_REF_CPU_CYCLES;

  typeMap["PERF_COUNT_SW_CPU_CLOCK"] = PERF_COUNT_SW_CPU_CLOCK;
  typeMap["PERF_COUNT_SW_TASK_CLOCK"] = PERF_COUNT_SW_TASK_CLOCK;
  typeMap["PERF_COUNT_SW_PAGE_FAULTS"] = PERF_COUNT_SW_PAGE_FAULTS;
  typeMap["PERF_COUNT_SW_CONTEXT_SWITCHES"] = PERF_COUNT_SW_CONTEXT_SWITCHES;
  typeMap["PERF_COUNT_SW_CPU_MIGRATIONS"] = PERF_COUNT_SW_CPU_MIGRATIONS;
  typeMap["PERF_COUNT_SW_PAGE_FAULTS_MIN"] = PERF_COUNT_SW_PAGE_FAULTS_MIN;
  typeMap["PERF_COUNT_SW_PAGE_FAULTS_MAJ"] = PERF_COUNT_SW_PAGE_FAULTS_MAJ;
  typeMap["PERF_COUNT_SW_ALIGNMENT_FAULTS"] = PERF_COUNT_SW_ALIGNMENT_FAULTS;
  typeMap["PERF_COUNT_SW_EMULATION_FAULTS"] = PERF_COUNT_SW_EMULATION_FAULTS;
  typeMap["PERF_COUNT_SW_BPF_OUTPUT"] = PERF_COUNT_SW_BPF_OUTPUT;
  typeMap["PERF_COUNT_SW_CGROUP_SWITCHES"] = PERF_COUNT_SW_CGROUP_SWITCHES;

  typeMap["PERF_COUNT_HW_CACHE_L1D_READ_ACCESS"] = PERF_COUNT_HW_CACHE_L1D | CACHE_ACCESS_R;
  typeMap["PERF_COUNT_HW_CACHE_L1D_READ_MISS"] = PERF_COUNT_HW_CACHE_L1D | CACHE_MISS_R;
  typeMap["PERF_COUNT_HW_CACHE_L1D_WRITE_ACCESS"] = PERF_COUNT_HW_CACHE_L1D | CACHE_ACCESS_W;
  typeMap["PERF_COUNT_HW_CACHE_L1D_WRITE_MISS"] = PERF_COUNT_HW_CACHE_L1D | CACHE_MISS_W;
  typeMap["PERF_COUNT_HW_CACHE_L1D_PREFETCH_ACCESS"] = PERF_COUNT_HW_CACHE_L1D| CACHE_ACCESS_P;
  typeMap["PERF_COUNT_HW_CACHE_L1D_PREFETCH_MISS"] = PERF_COUNT_HW_CACHE_L1D| CACHE_MISS_P;
  typeMap["PERF_COUNT_HW_CACHE_L1I_READ_ACCESS"] = PERF_COUNT_HW_CACHE_L1I| CACHE_ACCESS_R;
  typeMap["PERF_COUNT_HW_CACHE_L1I_READ_MISS"] = PERF_COUNT_HW_CACHE_L1I| CACHE_MISS_R;
  typeMap["PERF_COUNT_HW_CACHE_L1I_WRITE_ACCESS"] = PERF_COUNT_HW_CACHE_L1I| CACHE_ACCESS_W;
  typeMap["PERF_COUNT_HW_CACHE_L1I_WRITE_MISS"] = PERF_COUNT_HW_CACHE_L1I| CACHE_MISS_W;
  typeMap["PERF_COUNT_HW_CACHE_L1I_PREFETCH_ACCESS"] = PERF_COUNT_HW_CACHE_L1I| CACHE_ACCESS_P;
  typeMap["PERF_COUNT_HW_CACHE_L1I_PREFETCH_MISS"] = PERF_COUNT_HW_CACHE_L1I| CACHE_MISS_P;
  typeMap["PERF_COUNT_HW_CACHE_LL_READ_ACCESS"] = PERF_COUNT_HW_CACHE_LL| CACHE_ACCESS_R;
  typeMap["PERF_COUNT_HW_CACHE_LL_READ_MISS"] = PERF_COUNT_HW_CACHE_LL| CACHE_MISS_R;
  typeMap["PERF_COUNT_HW_CACHE_LL_WRITE_ACCESS"] = PERF_COUNT_HW_CACHE_LL| CACHE_ACCESS_W;
  typeMap["PERF_COUNT_HW_CACHE_LL_WRITE_MISS"] = PERF_COUNT_HW_CACHE_LL| CACHE_MISS_W;
  typeMap["PERF_COUNT_HW_CACHE_LL_PREFETCH_ACCESS"] = PERF_COUNT_HW_CACHE_LL| CACHE_ACCESS_P;
  typeMap["PERF_COUNT_HW_CACHE_LL_PREFETCH_MISS"] = PERF_COUNT_HW_CACHE_LL| CACHE_MISS_P;
  typeMap["PERF_COUNT_HW_CACHE_DTLB_READ_ACCESS"] = PERF_COUNT_HW_CACHE_DTLB| CACHE_ACCESS_R;
  typeMap["PERF_COUNT_HW_CACHE_DTLB_READ_MISS"] = PERF_COUNT_HW_CACHE_DTLB| CACHE_MISS_R;
  typeMap["PERF_COUNT_HW_CACHE_DTLB_WRITE_ACCESS"] = PERF_COUNT_HW_CACHE_DTLB| CACHE_ACCESS_W;
  typeMap["PERF_COUNT_HW_CACHE_DTLB_WRITE_MISS"] = PERF_COUNT_HW_CACHE_DTLB| CACHE_MISS_W;
  typeMap["PERF_COUNT_HW_CACHE_DTLB_PREFETCH_ACCESS"] = PERF_COUNT_HW_CACHE_DTLB| CACHE_ACCESS_P;
  typeMap["PERF_COUNT_HW_CACHE_DTLB_PREFETCH_MISS"] = PERF_COUNT_HW_CACHE_DTLB| CACHE_MISS_P;
  typeMap["PERF_COUNT_HW_CACHE_ITLB_READ_ACCESS"] = PERF_COUNT_HW_CACHE_ITLB| CACHE_ACCESS_R;
  typeMap["PERF_COUNT_HW_CACHE_ITLB_READ_MISS"] = PERF_COUNT_HW_CACHE_ITLB| CACHE_MISS_R;
  typeMap["PERF_COUNT_HW_CACHE_ITLB_WRITE_ACCESS"] = PERF_COUNT_HW_CACHE_ITLB| CACHE_ACCESS_W;
  typeMap["PERF_COUNT_HW_CACHE_ITLB_WRITE_MISS"] = PERF_COUNT_HW_CACHE_ITLB| CACHE_MISS_W;
  typeMap["PERF_COUNT_HW_CACHE_ITLB_PREFETCH_ACCESS"] = PERF_COUNT_HW_CACHE_ITLB| CACHE_ACCESS_P;
  typeMap["PERF_COUNT_HW_CACHE_ITLB_PREFETCH_MISS"] = PERF_COUNT_HW_CACHE_ITLB| CACHE_MISS_P;
  typeMap["PERF_COUNT_HW_CACHE_BPU_READ_ACCESS"] = PERF_COUNT_HW_CACHE_BPU| CACHE_ACCESS_R;
  typeMap["PERF_COUNT_HW_CACHE_BPU_READ_MISS"] = PERF_COUNT_HW_CACHE_BPU| CACHE_MISS_R;
  typeMap["PERF_COUNT_HW_CACHE_BPU_WRITE_ACCESS"] = PERF_COUNT_HW_CACHE_BPU| CACHE_ACCESS_W;
  typeMap["PERF_COUNT_HW_CACHE_BPU_WRITE_MISS"] = PERF_COUNT_HW_CACHE_BPU| CACHE_MISS_W;
  typeMap["PERF_COUNT_HW_CACHE_BPU_PREFETCH_ACCESS"] = PERF_COUNT_HW_CACHE_BPU| CACHE_ACCESS_P;
  typeMap["PERF_COUNT_HW_CACHE_BPU_PREFETCH_MISS"] = PERF_COUNT_HW_CACHE_BPU| CACHE_MISS_P;
  typeMap["PERF_COUNT_HW_CACHE_NODE_READ_ACCESS"] = PERF_COUNT_HW_CACHE_NODE| CACHE_ACCESS_R;
  typeMap["PERF_COUNT_HW_CACHE_NODE_READ_MISS"] = PERF_COUNT_HW_CACHE_NODE| CACHE_MISS_R;
  typeMap["PERF_COUNT_HW_CACHE_NODE_WRITE_ACCESS"] = PERF_COUNT_HW_CACHE_NODE| CACHE_ACCESS_W;
  typeMap["PERF_COUNT_HW_CACHE_NODE_WRITE_MISS"] = PERF_COUNT_HW_CACHE_NODE| CACHE_MISS_W;
  typeMap["PERF_COUNT_HW_CACHE_NODE_PREFETCH_ACCESS"] = PERF_COUNT_HW_CACHE_NODE| CACHE_ACCESS_P;
  typeMap["PERF_COUNT_HW_CACHE_NODE_PREFETCH_MISS"] = PERF_COUNT_HW_CACHE_NODE| CACHE_MISS_P;

  // clang-format on
  return;
}

int KProfEvent::TypeLookup(const std::string& query) {
  auto it = typeMap.find(query);
  if (it != typeMap.end()) {
    return it->second;
  }

  // try  to get the hex value
  try {
    if (query.substr(0, 2) == "0x" || query.substr(0, 2) == "0X")
      return std::stoi(query.substr(2), nullptr, 16);
    else
      return std::stoi(query.substr(2), nullptr, 10);
  } catch (std::invalid_argument&) {
    return -1;
  } catch (std::out_of_range&) {
    return -1;
  }
  // if nothing works we must still return error
  return -1;
}

void KProfEvent::ReadCounterList(const std::string& filename) {
  // Event file MUST be a csv file with the format
  // event_title,EVENT_TYPE,EVENT_NAME

  std::ifstream configFile(filename);
  if (!configFile.is_open()) {
    std::cerr << "Error opening file: " << filename << std::endl;
    return;
  }

  std::string line;
  int leader = -1;
  while (std::getline(configFile, line)) {
    std::stringstream ss(line);
    std::string name;
    std::string counterType;
    std::string counterSpec;

    if (std::getline(ss, name, ',') && std::getline(ss, counterType, ',') &&
        std::getline(ss, counterSpec)) {
      int type = TypeLookup(counterType);
      int spec = TypeLookup(counterSpec);
      if (type == -1) {
        std::cerr << "Invalid type specified in line " << line
                  << ". Ignoring counter." << std ::endl;
      } else if (spec == -1) {
        std::cerr << "Invalid counter specified in line " << line
                  << ". Ignoring counter." << std ::endl;

      } else {
        // Attempt to initialize counter
        // Force to userland for now
        RegisterCounter(name, leader, type, spec, USER);
      }
    }
  }

  // After this loop, if no events remain, throw an error
  if (events.size() == 0) {
    names.resize(0);
    throw std::runtime_error(
        "No counter is available. Please check your code/system!");
  }
}

void KProfEvent::ReadEnvConfig(bool configType, bool& foundStatus,
                               std::string& varVal) {
  const char* value = (configType) ? getenv("KPROF_COUNTER_FILE")
                                   : getenv("KPROF_COUNTER_CONF");
  if (!value) {
    foundStatus = false;
    varVal = std::string("");
    return;
  }

  foundStatus = true;
  varVal = std::string(value);
  return;
}

inline void ShowErrForToken(std::string& token) {
  std::cerr << "Invalid format in: " << token
            << ". Ignoring counter on this system." << std::endl;
}

int PerfTypeLookup(std::string& query) {
  if (query == "H") {
    return PERF_TYPE_HARDWARE;
  } else if (query == "S") {
    return PERF_TYPE_SOFTWARE;
  } else if (query == "C") {
    return PERF_TYPE_HW_CACHE;
  } else if (query == "R") {
    return PERF_TYPE_RAW;
  } else {
    return -1;
  }
}

void KProfEvent::ParseEnvConfig(std::string& parsedEnv) {
  // we are sure that the user has input a string here and we
  // need to find it syntax: name0,T0:VAL0;name0,T1:VAL1;...
  std::istringstream ss(parsedEnv);
  std::string token;
  std::vector<std::string> configList;

  std::vector<std::string> parsedList;
  while (std::getline(ss, token, ';')) {
    if (!token.empty()) configList.push_back(token);
  }

  // force only first counter to be leader
  int leader = -1;
  // now we can parse each config
  for (auto& token : configList) {
    // token structure is name,T:VAL;, name is a label str, T a type str and
    // VAL is a value str
    // find the comma
    auto commapos = token.find(',');
    if (commapos != std::string::npos) {
      std::string name = token.substr(0, commapos);      // get name
      std::string typeVal = token.substr(commapos + 1);  // the rest

      auto colonpos = typeVal.find(':');
      if (colonpos != std::string::npos) {
        std::string typestr = typeVal.substr(0, colonpos);
        std::string valuestr =
            typeVal.substr(colonpos + 1);  // we have the value now

        // now we can register the counter
        int type = PerfTypeLookup(typestr);
        int spec = TypeLookup(valuestr);

        if (type == -1) {
          std::cerr << "Invalid type specified at " << token
                    << ". Ignoring counter." << std ::endl;
        } else if (spec == -1) {
          std::cerr << "Invalid counter specified at " << token
                    << ". Ignoring counter." << std ::endl;

        } else {
          // Attempt to initialize counter
          RegisterCounter(name, leader, type, spec, USER);
        }
      } else {
        ShowErrForToken(token);
      }
    } else {
      ShowErrForToken(token);
    }
  }

  // loop for counter checking has ended here
  // After this loop, if no events remain, throw an error
  if (events.size() == 0) {
    names.resize(0);
    throw std::runtime_error(
        "No counter is available. Please check your code/system!");
  }
}

};  // namespace KProf