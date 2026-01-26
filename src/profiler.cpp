#include "profiler.hpp"

#include <algorithm>  // for std::find
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

void PerfEvent::RegisterCounter(const std::string& name, int& leaderFD,
                                uint64_t type, uint64_t eventID,
                                EventDomain domain = ALL) {
  // names.push_back(name);
  // events.push_back(Event());
  // auto& event = events.back();
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
  pe.exclude_idle = 1;
  pe.read_format =
      PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

  event.isLeader = (leaderFD == -1) ? true : false;

  bool secondCallWasNeeded = false;

  event.fd = static_cast<int>(
      syscall(SYS_perf_event_open, &event.pe, 0, -1, leaderFD, 0));
  if (event.fd < 0) {
    if (errno == 22) {
      std::cerr << "Could not open " << name
                << " with the specified leader. "
                   "Rettempting as leader."
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
    events.push_back(event);
    names.push_back(name);
    // event was successfully added
    if (event.isLeader) {
      event.leaderFD = event.fd;
      leaderFD = event.fd;  // send this to the next call to RegisterCounter
    } else {
      event.leaderFD = leaderFD;
    }
  }
}

void PerfEvent::StartCounters() {
  // could use a std::for_each but we need the index.
  // TODO: Define an enumerate()?
  for (size_t i = 0; i < events.size(); i++) {
    auto& event = events[i];
    auto& name = names[i];
    auto ret = ioctl(event.fd, PERF_EVENT_IOC_RESET, 0);
    if (ret == -1) {
      std::stringstream errmsg;
      errmsg << "PERF_EVENT_IOC_RESET failed for " << name << "! : " << errno
             << " " << DescribeError_IOCTL(errno);
      throw std::runtime_error(errmsg.str());
    }
    startTime = std::chrono::high_resolution_clock::now();
    ret = ioctl(event.fd, PERF_EVENT_IOC_ENABLE, 0);
    if (ret == -1) {
      std::stringstream errmsg;
      errmsg << "PERF_EVENT_IOC_ENABLE failed for " << name << "! : " << errno
             << " " << DescribeError_IOCTL(errno);
      throw std::runtime_error(errmsg.str());
    }
    //   if (read(event.fd, &event.prev, sizeof(uint64_t) * 3) !=
    //       sizeof(uint64_t) * 3)
    //     std::cerr << "Error reading counter " << names[i] << std::endl;
  }
}

void PerfEvent::StopCounters() {
  for (unsigned i = 0; i < events.size(); i++) {
    auto& event = events[i];
    auto ret = ioctl(event.fd, PERF_EVENT_IOC_DISABLE, 0);
    if (ret == -1) {
      std::stringstream errmsg;
      errmsg << "PERF_EVENT_IOC_DISABLE failed for " << names[i]
             << "! : " << errno << " " << DescribeError_IOCTL(errno);
      throw std::runtime_error(errmsg.str());
    }
    if (read(event.fd, &event.data, sizeof(uint64_t) * 3) !=
        sizeof(uint64_t) * 3)
      std::cerr << "Error reading counter " << names[i] << std::endl;
    stopTime = std::chrono::high_resolution_clock::now();
  }
}

long double PerfEvent::GetCounter(const std::string& name) {
  for (size_t i = 0; i < events.size(); i++)
    if (names[i] == name) return events[i].readCounter();
  return -1;
}

std::vector<EventType> PerfEvent::GetReport(long double scaleFactor = 1.0,
                                            bool overheadCorrection = false) {
  std::vector<EventType> report(names.size() + 1);
  for (size_t i = 0; i < report.size() - 1; ++i) {
    std::string name = names[i];
    long double count = GetCounter(names[i]) / scaleFactor;
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

void PerfEvent::PrintReport() {
  for (size_t i = 0; i < names.size(); ++i) {
    std::cout << std::format("{} : {}", names[i], GetCounter(names[i]))
              << std::endl;
  }
  return;
}

void PerfEvent::PrintReport(std::vector<EventType> report) {
  for (size_t i = 0; i < report.size(); ++i) {
    std::cout << std::format("{} : {}", report[i].GetName(),
                             report[i].GetCount())
              << std::endl;
  }
  return;
}

PerfEvent::PerfEvent() {
  // todo: lookup more counters in linux/perf_event.h

  // prevent formatter from messing with this
  // clang-format off

  int dummy = -1;
  RegisterCounter("HW-instructions"   , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS                , USER);
  RegisterCounter("CPU-cycles"        , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES                  , USER);
  RegisterCounter("Branch-instuctions", dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS         , USER);
  RegisterCounter("Branch-misses"     , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES               , USER);
  RegisterCounter("Bus-cycles"        , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES                  , USER);
  RegisterCounter("Stall-frontend"    , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND     , USER);
  RegisterCounter("Stall-backend"     , dummy, PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND      , USER);
  RegisterCounter("L1d-read-miss"     , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_MISS_R    , USER);
  RegisterCounter("L1d-write-miss"    , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_MISS_W    , USER);
  RegisterCounter("L1d-read-access"   , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_ACCESS_R  , USER);
  RegisterCounter("L1d-write-access"  , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_ACCESS_W  , USER);
  RegisterCounter("L1i-read-miss"     , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_MISS_R    , USER);
  RegisterCounter("L1i-write-miss"    , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_MISS_W    , USER);
  RegisterCounter("L1i-read-access"   , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_ACCESS_R  , USER);
  RegisterCounter("L1i-write-access"  , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_ACCESS_W  , USER);
  RegisterCounter("LLC-read-miss"     , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_MISS_R    , USER);
  RegisterCounter("LLC-write-miss"    , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_MISS_W    , USER);
  RegisterCounter("LLC-read-access"   , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_ACCESS_R  , USER);
  RegisterCounter("LLC-write-access"  , dummy, PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_ACCESS_W  , USER);
  RegisterCounter("Pagefaults-total"  , dummy, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS                 , USER);
  RegisterCounter("Pagefaults-maj"    , dummy, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ             , USER);
  RegisterCounter("Pagefaults-min"    , dummy, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN             , USER);
  RegisterCounter("Alignment-faults"  , dummy, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS            , USER);
  RegisterCounter("CPU-migrations"    , dummy, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS              , USER);

  // clang-format on

  return;
}

PerfEvent::PerfEvent(const std::string& configFile) {
  ConstructTypeMap();
  ReadCounterList(configFile);
}

PerfEvent::~PerfEvent() {
  for (auto& event : events) {
    close(event.fd);
  }
}

std::vector<EventType> PerfEvent::GetOverhead() {
  StartCounters();
  StopCounters();
  return GetReport(1.0, false);
}

void PerfEvent::ConstructTypeMap() {
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
  typeMap["PERF_COUNT_SW_DUMMY"] = PERF_COUNT_SW_DUMMY;
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

int PerfEvent::TypeLookup(const std::string& query) {
  auto it = typeMap.find(query);
  if (it != typeMap.end()) {
    return it->second;
  }

  // try  to get the hex value
  try {
    if (query.substr(0, 2) == "0x" || query.substr(0, 2) == "0X")
      return std::stoi(query.substr(2), nullptr, 16);
  } catch (std::invalid_argument&) {
    return -1;
  } catch (std::out_of_range&) {
    return -1;
  }
  // if nothing works we must still return error
  return -1;
}

void PerfEvent::ReadCounterList(const std::string& filename) {
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
      uint64_t type = TypeLookup(counterType);
      uint64_t spec = TypeLookup(counterSpec);
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
