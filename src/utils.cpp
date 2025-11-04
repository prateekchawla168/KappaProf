#include <vector>
#include <random>
#include <stdexcept>
#include <fstream>
#include <string>
#include <format>
#include <iostream>
#include <sstream>
#include "utils.hpp"

long double average(const std::vector<long double> &arr)
{
    if (arr.empty())
    {
        throw std::invalid_argument("Trying to calculate average of empty vector.");
    }

    long double sum = 0.0;
    for (const auto &number : arr)
    {
        sum += number;
    }
    return sum / arr.size();
}

std::vector<long double> average(const std::vector<std::vector<long long>> &arr)
{
    size_t num_columns = arr[0].size();
    size_t num_rows = arr.size();
    std::vector<long long> col_sums(num_columns, 0ll);
    std::vector<long double> averages(num_columns, 0.0);

    for (const auto &row : arr)
    {
        for (size_t j = 0; j < num_columns; ++j)
        {
            col_sums[j] += row[j];
        }
    }

    for (size_t j = 0; j < num_columns; ++j)
    {
        averages[j] = static_cast<long double>(col_sums[j]) / (long double)num_rows;
    }

    return averages;
}

void dump_csv(const std::string &test_type, int M, int N, int K, long double avg_cycles, long double avg_time)
{
    std::ofstream csvfile;
    csvfile.open("comparison.csv", std::ios::app);

    if (!csvfile)
    {
        std::cerr << "Error opening file " << std::endl;
        return;
    }

    csvfile.seekp(0, std::ios::end);
    bool is_empty = (csvfile.tellp() == 0);
    if (is_empty)
    {
        csvfile << "Test_type,M,N,K,avg_cycles,avg_time,FLOPs_per_cycle,GFLOPS_per_sec\n";
    }
    // std::cout << "is empty? " << is_empty << std::endl;
    auto flops = (long double)2.0 * M * N * K;
    auto line = std::format("{},{},{},{},{},{},{},{}\n", test_type, M, N, K, avg_cycles, avg_time, flops / avg_cycles, flops / avg_time);

    csvfile << line;

    std::cout << " String written to file\n";

    csvfile.close();
}

void display_csv()
{
    std::ifstream csvfile;
    csvfile.open("comparison.csv");
    if (!csvfile)
    {
        std::cerr << "Error opening file. " << std::endl;
        return;
    }
    csvfile.seekg(0, std::ios::beg);
    bool is_empty = (csvfile.tellg() == 0 && (csvfile.peek() == std::ifstream::traits_type::eof()));

    if (is_empty)
    {
        std::cout << "File is empty!" << std::endl;
        return;
    }

    // for now, read row by row and display every word, removing commas
    std::vector<std::vector<std::string>> data;
    std::string line;
    while (std::getline(csvfile, line))
    {
        std::vector<std::string> row;
        std::stringstream ss(line);
        std::string element;

        while (std::getline(ss, element, ','))
        {
            row.push_back(element);
        }

        data.push_back(row);
    }

    csvfile.close();

    std::cout << std::string(30, '-') << std::endl;
    std::cout << "Contents of CSV file: " << std::endl;
    // display everything
    for (const auto &row : data)
    {
        for (const auto &el : row)
        {
            std::cout << el << "\t";
        }
        std::cout << std::endl;
    }
    std::cout << std::string(30, '-') << std::endl;
    return;
}