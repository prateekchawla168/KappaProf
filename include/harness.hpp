#pragma once

#include <chrono>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

#include "profiler.hpp"
#include "utils.hpp"

struct BenchmarkResult {
    int m;
    int n;
    int k;
    int runs;
    int warmupRuns;
    std::string name;
    std::vector<std::vector<EventType>> reports;
    std::vector<long double> timeDurations;

    void PrintReport();
    void DumpCSV();
};

template <typename T>
using GemmFunctionPointer = void (*)(int, int, int, T, const std::vector<T>&, const std::vector<T>&, T, std::vector<T>&);

template <typename T>
class Harness {
    // test  harness to run a gemm test

   public:
    Harness() { m_monitor = new PerfEvent(); }
    Harness(int _m, int _n, int _k, int _runs, T _alpha, T _beta, GemmFunctionPointer<T> _funcPtr, std::string _label)
        : m(_m), n(_n), k(_k), runs(_runs), alpha(_alpha), beta(_beta), funcPtr(_funcPtr), test_label(_label) {
        m_monitor = new PerfEvent();
    }
    BenchmarkResult RunBenchmark();

    ~Harness() {
        // release memory for the monitor instance
        delete m_monitor;
        m_monitor = nullptr;
    }

   protected:
    int m = 1;
    int n = 1;
    int k = 1;
    int runs = 1;
    int warmupRuns = 20;
    T alpha = 0.0;
    T beta = 0.0;
    GemmFunctionPointer<T> funcPtr;
    std::string test_label;
    PerfEvent* m_monitor;
};

// need to include templated functions in the header itself
template <typename T>
BenchmarkResult Harness<T>::RunBenchmark() {
    // std::clog << "Running benchmark for " << test_label << std::endl;
    // we already have initialized members, we need to get the test harness ready
    std::vector<T> A = generate_random_matrix<T>(m, k);
    std::vector<T> B = generate_random_matrix<T>(k, n);
    std::vector<T> C = generate_random_matrix<T>(m, n);

    std::vector<std::vector<EventType>> monitor_reports;
    std::vector<long double> timeDurations;

    for (auto r : std::views::iota(0, runs + warmupRuns)) {
        if (r > 0) {
            A = generate_random_matrix<T>(m, k);
            B = generate_random_matrix<T>(k, n);
            C = generate_random_matrix<T>(m, n);
        }

        m_monitor->StartCounters();
        funcPtr(m, n, k, alpha, A, B, beta, C);
        m_monitor->StopCounters();
        std::vector<EventType> run_report = m_monitor->GetReport();
        monitor_reports.emplace_back(run_report);
        timeDurations.emplace_back(m_monitor->GetDuration());
        // std::clog << std::format("Completed test {}/{} in {}s.", r + 1, runs + warmupRuns, timeDurations.back()) << std::endl;
    }

    // std::clog << "Completed. " << std::endl;

    BenchmarkResult res;
    res.m = m;
    res.n = n;
    res.k = k;
    res.runs = runs;
    res.warmupRuns = warmupRuns;
    res.name = test_label;
    res.reports = monitor_reports;
    res.timeDurations = timeDurations;

    return res;
}

template <typename T>
using KokkosGemmPtr = void (*)(int, int, int, T, const Kokkos::View<T**>& A, const Kokkos::View<T**>& B, T beta, Kokkos::View<T**>& C);

template <typename T>
class KokkosHarness : public Harness<T> {
    // test  harness to run a gemm test

   public:
    KokkosHarness() { m_monitor = new PerfEvent(); }
    KokkosHarness(int _m, int _n, int _k, int _runs, int _warmupRuns, T _alpha, T _beta, KokkosGemmPtr<T> _funcPtr, std::string _label)
        : m(_m), n(_n), k(_k), runs(_runs), warmupRuns(_warmupRuns), alpha(_alpha), beta(_beta), funcPtr(_funcPtr), test_label(_label) {
        m_monitor = new PerfEvent();
    }
    BenchmarkResult RunBenchmark();

    ~KokkosHarness() {
        // release memory for the monitor instance
        delete m_monitor;
        m_monitor = nullptr;
    }

   protected:
    int m = 1;
    int n = 1;
    int k = 1;
    int runs = 1;
    int warmupRuns = 1;
    T alpha = 0.0;
    T beta = 0.0;
    KokkosGemmPtr<T> funcPtr;
    std::string test_label;
    PerfEvent* m_monitor;
};

// need to include templated functions in the header itself
template <typename T>
BenchmarkResult KokkosHarness<T>::RunBenchmark() {
    // we already have initialized members, we need to get the test harness ready
    Kokkos::View<float**> A("A", m, k);
    Kokkos::View<float**> B("B", k, n);
    Kokkos::View<float**> C("C", m, n);

    std::vector<std::vector<EventType>> monitor_reports;
    std::vector<long double> timeDurations;

    // std::clog << "Running benchmark for " << test_label << std::endl;
    for (auto r : std::views::iota(0, runs + warmupRuns)) {
        if (r > 0) {
            GenerateRandomView(A);
            GenerateRandomView(B);
            GenerateRandomView(C);
        }

        m_monitor->StartCounters();
        funcPtr(m, n, k, alpha, A, B, beta, C);
        m_monitor->StopCounters();
        std::vector<EventType> run_report = m_monitor->GetReport();
        monitor_reports.emplace_back(run_report);
        timeDurations.emplace_back(m_monitor->GetDuration());
        // std::clog << std::format("Completed test {}/{} in {}s.", r + 1, runs + warmupRuns, timeDurations.back()) << std::endl;
    }
    std::clog << "Completed. " << std::endl;

    BenchmarkResult res;
    res.m = m;
    res.n = n;
    res.k = k;
    res.runs = runs;
    res.warmupRuns = warmupRuns;
    res.name = test_label;
    res.reports = monitor_reports;
    res.timeDurations = timeDurations;

    return res;
}
