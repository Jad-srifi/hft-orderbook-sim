#include <benchmark.hpp>
#include <algorithm>
#include <stdexcept>

BenchmarkResult run_benchmark(const BenchmarkName name, const std::function<void()> &workload, std::size_t operation_count, const BenchmarkConfig &config)
{
    if (config.measured_run_count == 0 || operation_count == 0)
    {
        throw std::invalid_argument("measured_run_count and operation_count must be greater than zero");
    }

    for (size_t i = 0; i < config.warmup_run_count; i++)
    {
        workload();
    }

    std::vector<BenchmarkDuration> durations;
    durations.reserve(config.measured_run_count);

    BenchmarkDuration max_duration;
    BenchmarkDuration min_duration;
    BenchmarkDuration total_duration{0};

    for (size_t i = 0; i < config.measured_run_count; i++)
    {
        auto start = std::chrono::steady_clock::now();

        workload();

        auto end = std::chrono::steady_clock::now();

        BenchmarkDuration duration = std::chrono::duration_cast<BenchmarkDuration>(end - start);
        durations.push_back(duration);

        total_duration += duration;

        if (i == 0)
        {
            max_duration = duration;
            min_duration = duration;
        }
        else
        {
            if (duration > max_duration)
            {
                max_duration = duration;
            }
            if (duration < min_duration)
            {
                min_duration = duration;
            }
        }
    }

    std::sort(durations.begin(), durations.end());

    BenchmarkResult result;

    result.name = name;

    result.max_duration = max_duration;
    result.min_duration = min_duration;

    if (durations.size() % 2 == 1)
    {
        result.median_duration = durations[durations.size() / 2];
    }

    else
    {
        auto lower = durations[durations.size() / 2 - 1];
        auto upper = durations[durations.size() / 2];

        result.median_duration = BenchmarkDuration{(lower.count() + upper.count()) / 2};
    }

    result.total_duration = total_duration;

    std::chrono::duration<double> seconds = result.median_duration;
    double nanoseconds = result.median_duration.count();

    result.operations = operation_count;
    result.nanoseconds_per_operation = nanoseconds / operation_count;

    result.throughput = static_cast<double>(operation_count / seconds.count());

    return result;
}