#pragma once

#include <types.hpp>

struct BenchmarkResult
{
    BenchmarkName name;
    std::size_t operations;
    BenchmarkDuration total_duration;
    BenchmarkDuration min_duration;
    BenchmarkDuration median_duration;
    BenchmarkDuration max_duration;
    OperationsPerSecond throughput;
    NanosecondsPerOperation nanoseconds_per_operation;
};

struct BenchmarkConfig
{
    std::size_t warmup_run_count;
    std::size_t measured_run_count;
};

BenchmarkResult run_benchmark(const BenchmarkName name, const std::function<void()> &workload, std::size_t operation_count, const BenchmarkConfig &config);