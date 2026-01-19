#include "profiler.hpp"

#include <algorithm>  // for std::find

void PerfEvent::RegisterCounter(const std::string& name, uint64_t type,
                                uint64_t eventID, EventDomain domain = ALL) {
  names.push_back(name);
  events.push_back(Event());
  auto& event = events.back();
  auto& pe = event.pe;
  memset(&pe, 0, sizeof(struct perf_event_attr));
  pe.type = static_cast<uint32_t>(type);
  pe.size = sizeof(struct perf_event_attr);
  pe.config = eventID;
  pe.disabled = true;
  pe.inherit = 1;
  pe.inherit_stat = 0;
  pe.exclude_user = !(domain & USER);
  pe.exclude_kernel = !(domain & KERNEL);
  pe.exclude_hv = !(domain & HYPERVISOR);
  pe.read_format =
      PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
}

void PerfEvent::StartCounters() {
  // could use a std::for_each but we need the index.
  // TODO: Define an enumerate() ?
  for (size_t i = 0; i < events.size(); i++) {
    auto& event = events[i];
    auto ret = ioctl(event.fd, PERF_EVENT_IOC_RESET, 0);
    if (ret == -1) {
      std::stringstream errmsg;
      errmsg << "PERF_EVENT_IOC_RESET failed! : " << errno << " "
             << strerror(errno);
      throw std::runtime_error(errmsg.str());
    }
    ret = ioctl(event.fd, PERF_EVENT_IOC_ENABLE, 0);
    if (ret == -1) {
      std::stringstream errmsg;
      errmsg << "PERF_EVENT_IOC_ENABLE failed! : " << errno << " "
             << strerror(errno);
      throw std::runtime_error(errmsg.str());
    }
    if (read(event.fd, &event.prev, sizeof(uint64_t) * 3) !=
        sizeof(uint64_t) * 3)
      std::cerr << "Error reading counter " << names[i] << std::endl;
  }
  startTime = std::chrono::high_resolution_clock::now();
}

void PerfEvent::StopCounters() {
  stopTime = std::chrono::high_resolution_clock::now();
  for (unsigned i = 0; i < events.size(); i++) {
    auto& event = events[i];
    if (read(event.fd, &event.data, sizeof(uint64_t) * 3) !=
        sizeof(uint64_t) * 3)
      std::cerr << "Error reading counter " << names[i] << std::endl;
    ioctl(event.fd, PERF_EVENT_IOC_DISABLE, 0);
  }
}

long double PerfEvent::GetCounter(const std::string& name) {
  for (size_t i = 0; i < events.size(); i++)
    if (names[i] == name) return events[i].readCounter();
  return -1;
}

std::vector<EventType> PerfEvent::GetReport(long double scaleFactor = 1.0) {
  std::vector<EventType> report(names.size());
  for (size_t i = 0; i < report.size(); ++i) {
    std::string name = names[i];
    long double count = GetCounter(names[i]) / scaleFactor;
    report[i].SetName(name);
    report[i].SetCount(count);
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

PerfEvent::PerfEvent() {
// todo: lookup more counters in linux/perf_event.h

// prevent formatter from messing with this
// clang-format off
  #define CACHE_MISS_R    ((PERF_COUNT_HW_CACHE_OP_READ  << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS   << 16))
  #define CACHE_MISS_W    ((PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS   << 16))
  #define CACHE_ACCESS_R  ((PERF_COUNT_HW_CACHE_OP_READ  << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))
  #define CACHE_ACCESS_W  ((PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))
  
  RegisterCounter("HW-instructions"   , PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS                , USER);
  RegisterCounter("CPU-cycles"        , PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES                  , USER);
  RegisterCounter("Branch-instuctions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS         , USER);
  RegisterCounter("Branch-misses"     , PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES               , USER);
  RegisterCounter("Bus-cycles"        , PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES                  , USER);
  RegisterCounter("Stall-frontend"    , PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND     , USER);
  RegisterCounter("Stall-backend"     , PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND      , USER);
  
  RegisterCounter("L1d-read-miss"     , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_MISS_R    , USER);
  RegisterCounter("L1d-write-miss"    , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_MISS_W    , USER);
  RegisterCounter("L1d-read-access"   , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_ACCESS_R  , USER);
  RegisterCounter("L1d-write-access"  , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | CACHE_ACCESS_W  , USER);
  RegisterCounter("L1i-read-miss"     , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_MISS_R    , USER);
  RegisterCounter("L1i-write-miss"    , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_MISS_W    , USER);
  RegisterCounter("L1i-read-access"   , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_ACCESS_R  , USER);
  RegisterCounter("L1i-write-access"  , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | CACHE_ACCESS_W  , USER);
  RegisterCounter("LLC-read-miss"     , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_MISS_R    , USER);
  RegisterCounter("LLC-write-miss"    , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_MISS_W    , USER);
  RegisterCounter("LLC-read-access"   , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_ACCESS_R  , USER);
  RegisterCounter("LLC-write-access"  , PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL  | CACHE_ACCESS_W  , USER);

  RegisterCounter("Pagefaults-total"  , PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS                 , USER);
  RegisterCounter("Pagefaults-maj"    , PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ             , USER);
  RegisterCounter("Pagefaults-min"    , PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN             , USER);
  RegisterCounter("Alignment-faults"  , PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS            , USER);
  RegisterCounter("CPU-migrations"    , PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS              , USER);

  // clang-format on

  for (size_t i = 0; i < events.size(); i++) {
    auto& event = events[i];
    auto& name = names[i];
    event.fd =
        static_cast<int>(syscall(SYS_perf_event_open, &event.pe, 0, -1, -1, 0));
    if (event.fd < 0) {
      std::cerr << "Error " << errno << " opening counter " << names[i] << ": "
                << strerror(errno)
                << ". Ignoring this counter on the current system. "
                << std::endl;
      // events.resize(0);
      // names.resize(0);
      auto it_events = std::find(events.begin(), events.end(), event);
      auto it_names = std::find(names.begin(), names.end(), name);

      if (it_events != events.end()) events.erase(it_events);
      if (it_names != names.end()) names.erase(it_names);

      // return;
    }
  }

  // After this loop, if no events remain, throw an error
  if (events.size() == 0) {
    names.resize(0);
    throw std::runtime_error(
        "No counter is available. Please check your code/system!");
  }
  return;
}

PerfEvent::~PerfEvent() {
  for (auto& event : events) {
    close(event.fd);
  }
}