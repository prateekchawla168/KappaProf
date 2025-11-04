#pragma once

#include <Kokkos_Core.hpp>
#include <Kokkos_Random.hpp>
#include <vector>
#include <string>
#include <random>
#include <chrono>

template <typename T>
std::vector<T> generate_random_matrix(int rows, int cols)
{
    std::vector<T> matrix(rows * cols, 0.0);
    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_real_distribution<T> distr(0.0f, 9.9f);

    for (auto &value : matrix)
    {
        value = distr(eng);
    }
    return matrix;
}

template <typename T>
void GenerateRandomView(Kokkos::View<T **> &input)
{
    auto timepoint = std::chrono::system_clock::now();
    Kokkos::Random_XorShift64_Pool<> rand_pool(std::chrono::duration_cast<std::chrono::microseconds>(timepoint.time_since_epoch()).count()); // optional seed
    Kokkos::fill_random(input, rand_pool, 10.0);                                                                                             // <execspace>, view, pool, range
    return;
}

long double average(const std::vector<long double> &arr);
std::vector<long double> average(const std::vector<std::vector<long long>> &arr);
void dump_csv(const std::string &test_type, int M, int N, int K, long double avg_cycles, long double avg_time);
void display_csv();