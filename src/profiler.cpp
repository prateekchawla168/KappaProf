#include "profiler.hpp"

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
      errmsg << "PERF_EVENT_IOC_RESET failed! " << strerror(errno);
      throw std::runtime_error(errmsg.str());
    }
    ret = ioctl(event.fd, PERF_EVENT_IOC_ENABLE, 0);
    if (ret == -1) {
      std::stringstream errmsg;
      errmsg << "PERF_EVENT_IOC_ENABLE failed! " << strerror(errno);
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
  // for now just manually add everything we can
  RegisterCounter("cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, USER);
  RegisterCounter("instructions", PERF_TYPE_HARDWARE,
                  PERF_COUNT_HW_INSTRUCTIONS, USER);
  RegisterCounter("L1-misses", PERF_TYPE_HW_CACHE,
                  PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                      (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
                  USER);
  RegisterCounter("LLC-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES,
                  USER);
  RegisterCounter("branch-misses", PERF_TYPE_HARDWARE,
                  PERF_COUNT_HW_BRANCH_MISSES, USER);
  // todo: lookup more counters in linux/perf_event.h

  for (size_t i = 0; i < events.size(); i++) {
    auto& event = events[i];
    event.fd =
        static_cast<int>(syscall(SYS_perf_event_open, &event.pe, 0, -1, -1, 0));
    if (event.fd < 0) {
      std::stringstream errmsg;
      errmsg << "Error opening counter " << names[i] << ": " << strerror(errno);
      throw std::runtime_error(errmsg.str());
      std::cerr << "Error opening counter " << names[i] << std::endl;
      events.resize(0);
      names.resize(0);
      return;
    }
  }
}

PerfEvent::~PerfEvent() {
  for (auto& event : events) {
    close(event.fd);
  }
}