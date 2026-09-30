#include <benchmark.hpp>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <stdexcept>

int main()
{
    // ------------------------------------------------------------
    // Test 1: Basic single-run benchmark
    // ------------------------------------------------------------

    {
        std::size_t call_count = 0;

        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 1;

        BenchmarkResult result = run_benchmark(
            "single_run",
            [&]()
            {
                ++call_count;
            },
            100,
            config);

        assert(call_count == 1);

        assert(result.name == "single_run");
        assert(result.operations == 100);

        assert(result.total_duration.count() >= 0);
        assert(result.min_duration.count() >= 0);
        assert(result.median_duration.count() >= 0);
        assert(result.max_duration.count() >= 0);

        assert(result.min_duration == result.median_duration);
        assert(result.median_duration == result.max_duration);

        assert(result.nanoseconds_per_operation >= 0.0);
        assert(result.throughput >= 0.0);
    }

    // ------------------------------------------------------------
    // Test 2: Warmup runs are executed but not measured
    // ------------------------------------------------------------

    {
        std::size_t call_count = 0;

        BenchmarkConfig config;
        config.warmup_run_count = 3;
        config.measured_run_count = 2;

        BenchmarkResult result = run_benchmark(
            "warmup",
            [&]()
            {
                ++call_count;
            },
            50,
            config);

        assert(call_count == 5);

        assert(result.name == "warmup");
        assert(result.operations == 50);
        assert(result.total_duration.count() >= 0);
    }

    // ------------------------------------------------------------
    // Test 3: Multiple measured runs
    // ------------------------------------------------------------

    {
        std::size_t call_count = 0;

        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 5;

        BenchmarkResult result = run_benchmark(
            "multiple_runs",
            [&]()
            {
                ++call_count;
            },
            1000,
            config);

        assert(call_count == 5);

        assert(result.operations == 1000);

        assert(result.min_duration <= result.median_duration);
        assert(result.median_duration <= result.max_duration);

        assert(result.total_duration.count() >= 0);
    }

    // ------------------------------------------------------------
    // Test 4: Even number of measured runs
    // ------------------------------------------------------------

    {
        std::size_t call_count = 0;

        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 4;

        BenchmarkResult result = run_benchmark(
            "even_runs",
            [&]()
            {
                ++call_count;
            },
            100,
            config);

        assert(call_count == 4);

        assert(result.min_duration <= result.median_duration);
        assert(result.median_duration <= result.max_duration);

        assert(result.total_duration.count() >= 0);
    }

    // ------------------------------------------------------------
    // Test 5: Odd number of measured runs
    // ------------------------------------------------------------

    {
        std::size_t call_count = 0;

        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 5;

        BenchmarkResult result = run_benchmark(
            "odd_runs",
            [&]()
            {
                ++call_count;
            },
            250,
            config);

        assert(call_count == 5);

        assert(result.min_duration <= result.median_duration);
        assert(result.median_duration <= result.max_duration);
    }

    // ------------------------------------------------------------
    // Test 6: Operation count is metadata, not workload repetition
    // ------------------------------------------------------------

    {
        std::size_t call_count = 0;

        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 3;

        BenchmarkResult result = run_benchmark(
            "operation_count",
            [&]()
            {
                ++call_count;
            },
            1000000,
            config);

        // The workload must run once per measured run,
        // NOT operation_count times.
        assert(call_count == 3);

        assert(result.operations == 1000000);
    }

    // ------------------------------------------------------------
    // Test 7: Total duration is non-negative
    // ------------------------------------------------------------

    {
        BenchmarkConfig config;
        config.warmup_run_count = 1;
        config.measured_run_count = 5;

        BenchmarkResult result = run_benchmark(
            "duration",
            []()
            {
                volatile std::size_t value = 0;

                for (std::size_t i = 0; i < 1000; ++i)
                {
                    value += i;
                }

                (void)value;
            },
            1000,
            config);

        assert(result.total_duration.count() >= 0);
        assert(result.min_duration.count() >= 0);
        assert(result.median_duration.count() >= 0);
        assert(result.max_duration.count() >= 0);
    }

    // ------------------------------------------------------------
    // Test 8: Timing ordering invariants
    // ------------------------------------------------------------

    {
        BenchmarkConfig config;
        config.warmup_run_count = 2;
        config.measured_run_count = 7;

        BenchmarkResult result = run_benchmark(
            "timing_order",
            []()
            {
                volatile std::size_t value = 0;

                for (std::size_t i = 0; i < 5000; ++i)
                {
                    value += i;
                }

                (void)value;
            },
            5000,
            config);

        assert(result.min_duration <= result.median_duration);
        assert(result.median_duration <= result.max_duration);

        assert(result.total_duration >= result.min_duration);
        assert(result.total_duration >= result.median_duration);
        assert(result.total_duration >= result.max_duration);
    }

    // ------------------------------------------------------------
    // Test 9: ns/op calculation is valid
    // ------------------------------------------------------------

    {
        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 3;

        BenchmarkResult result = run_benchmark(
            "ns_per_operation",
            []()
            {
                volatile std::size_t value = 0;

                for (std::size_t i = 0; i < 1000; ++i)
                {
                    value += i;
                }

                (void)value;
            },
            1000,
            config);

        assert(result.nanoseconds_per_operation >= 0.0);
        assert(std::isfinite(result.nanoseconds_per_operation));
    }

    // ------------------------------------------------------------
    // Test 10: Throughput calculation is valid
    // ------------------------------------------------------------

    {
        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 3;

        BenchmarkResult result = run_benchmark(
            "throughput",
            []()
            {
                volatile std::size_t value = 0;

                for (std::size_t i = 0; i < 1000; ++i)
                {
                    value += i;
                }

                (void)value;
            },
            1000,
            config);

        assert(result.throughput >= 0.0);
        assert(std::isfinite(result.throughput));
    }

    // ------------------------------------------------------------
    // Test 11: Zero measured runs must throw
    // ------------------------------------------------------------

    {
        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 0;

        bool threw = false;

        try
        {
            run_benchmark(
                "zero_measured_runs",
                []() {},
                100,
                config);
        }
        catch (const std::invalid_argument &)
        {
            threw = true;
        }

        assert(threw);
    }

    // ------------------------------------------------------------
    // Test 12: Zero operation count must throw
    // ------------------------------------------------------------

    {
        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 1;

        bool threw = false;

        try
        {
            run_benchmark(
                "zero_operations",
                []() {},
                0,
                config);
        }
        catch (const std::invalid_argument &)
        {
            threw = true;
        }

        assert(threw);
    }

    // ------------------------------------------------------------
    // Test 13: Warmup-only count does not affect measured count
    // ------------------------------------------------------------

    {
        std::size_t call_count = 0;

        BenchmarkConfig config;
        config.warmup_run_count = 10;
        config.measured_run_count = 1;

        run_benchmark(
            "warmup_only_effect",
            [&]()
            {
                ++call_count;
            },
            10,
            config);

        assert(call_count == 11);
    }

    // ------------------------------------------------------------
    // Test 14: Different operation counts are preserved
    // ------------------------------------------------------------

    {
        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 2;

        BenchmarkResult result_a = run_benchmark(
            "operations_a",
            []() {},
            1,
            config);

        BenchmarkResult result_b = run_benchmark(
            "operations_b",
            []() {},
            100000,
            config);

        assert(result_a.operations == 1);
        assert(result_b.operations == 100000);
    }

    // ------------------------------------------------------------
    // Test 15: Benchmark name is preserved
    // ------------------------------------------------------------

    {
        BenchmarkConfig config;
        config.warmup_run_count = 0;
        config.measured_run_count = 1;

        BenchmarkResult result = run_benchmark(
            "OrderBook::add_order",
            []() {},
            100,
            config);

        assert(result.name == "OrderBook::add_order");
    }

    std::cout << "All benchmark tests passed.\n";

    return 0;
}