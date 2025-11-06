#include "harness.hpp"

template <typename T>
void BenchmarkResult<T>::DumpCSV() {
    std::ofstream csvfile;
    csvfile.open("comparison.csv", std::ios::app);

    if (!csvfile) {
        std::cerr << "Error opening file " << std::endl;
        return;
    }

    csvfile.seekp(0, std::ios::end);
    bool is_empty = (csvfile.tellp() == 0);
    if (is_empty) {
        std::vector<std::string> name_list;
        for (auto& monitor : reports[0])  // monitor -> pair
            name_list.emplace_back(monitor.GetName());

        std::string names_formatted = "";
        auto separator = ",";
        for (const auto& name_el : name_list) names_formatted += name_el + std::string(separator);
        names_formatted.pop_back();  // remove last comma

        csvfile << "Test_type,M,N,K,alpha,beta,flops,wall-time," << names_formatted << "\n";
    }

    // std::cout << "is empty? " << is_empty << std::endl;
    auto flops = (long double)2.0 * m * n * k;
    for (auto run : std::views::iota(warmupRuns, warmupRuns + runs)) {
        // get all the counters we have
        std::vector<long double> counter_list;
        for (auto& monitor : reports[run]) counter_list.emplace_back(monitor.GetCount());
        // format into a csv line
        std::string counts_formatted = "";
        auto separator = ",";
        for (auto& count : counter_list) counts_formatted += std::to_string(count) + std::string(separator);
        counts_formatted.pop_back();  // remove last comma

        /* First counter is hw cycles by default */
        auto line = std::format("{},{},{},{},{},{},{},{},{}\n", name, m, n, k, alpha, beta, flops, timeDurations[run], counts_formatted);

        csvfile << line;
    }

    csvfile.close();
}

template <typename T>
void BenchmarkResult<T>::PrintReport() {
    std::cout << std::string(30, '-') << std::endl;
    std::cout << "\tSIMULATION DETAILS : " << std::endl;
    std::cout << std::format("Name of test: {}", name) << std::endl;
    std::cout << std::format("\t M = {} \t N = {} \t K = {}", m, n, k) << std::endl;

    std::vector<std::string> name_list;
    for (auto& monitor : reports[0])  // monitor -> pair
        name_list.emplace_back(monitor.GetName());

    std::string names_formatted = "";
    auto separator = "\t";
    for (const auto& name_el : name_list) names_formatted += name_el + std::string(separator);

    std::cout << "Runs\t" << names_formatted << std::endl;

    // now get a formatted list of times:
    for (auto run : std::views::iota(0, runs)) {
        std::vector<long double> counter_list;
        for (auto& monitor : reports[run]) counter_list.emplace_back(monitor.GetCount());

        std::string counts_formatted = "";
        auto separator = "\t";
        for (auto& count : counter_list) counts_formatted += std::to_string(count) + std::string(separator);

        std::cout << run << separator << counts_formatted << std::endl;
    }

    std::cout << std::string(30, '-') << std::endl;
}