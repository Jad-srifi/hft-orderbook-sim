#include <profile.hpp>

#include <iostream>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace
{

    const ProfileResult *find_result(
        const std::vector<ProfileResult> &results,
        const std::string &name)
    {
        for (const auto &result : results)
        {
            if (result.name == name)
            {
                return &result;
            }
        }

        return nullptr;
    }

    void test_empty_profiler()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        const auto results = profiler.results();

        assert(results.empty());
        assert(profiler.total_profiled_ns() == 0);
    }

    void test_single_record()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("OrderBook::add", 1000);

        const auto results = profiler.results();

        assert(results.size() == 1);

        const ProfileResult *result =
            find_result(results, "OrderBook::add");

        assert(result != nullptr);
        assert(result->calls == 1);
        assert(result->total_ns == 1000);
        assert(result->min_ns == 1000);
        assert(result->max_ns == 1000);
        assert(result->avg_ns == 1000);
        assert(std::abs(result->share_percent - 100.0) < 1e-9);

        assert(profiler.total_profiled_ns() == 1000);
    }

    void test_multiple_records_same_scope()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("OrderBook::add", 1000);
        profiler.record("OrderBook::add", 2000);
        profiler.record("OrderBook::add", 3000);

        const auto results = profiler.results();

        assert(results.size() == 1);

        const ProfileResult *result =
            find_result(results, "OrderBook::add");

        assert(result != nullptr);
        assert(result->calls == 3);
        assert(result->total_ns == 6000);
        assert(result->min_ns == 1000);
        assert(result->max_ns == 3000);
        assert(result->avg_ns == 2000);

        assert(std::abs(result->share_percent - 100.0) < 1e-9);
        assert(profiler.total_profiled_ns() == 6000);
    }

    void test_multiple_scopes()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("OrderBook::add", 1000);
        profiler.record("OrderBook::cancel", 2000);
        profiler.record("OrderBook::modify", 3000);

        const auto results = profiler.results();

        assert(results.size() == 3);

        const ProfileResult *add =
            find_result(results, "OrderBook::add");

        const ProfileResult *cancel =
            find_result(results, "OrderBook::cancel");

        const ProfileResult *modify =
            find_result(results, "OrderBook::modify");

        assert(add != nullptr);
        assert(cancel != nullptr);
        assert(modify != nullptr);

        assert(add->total_ns == 1000);
        assert(cancel->total_ns == 2000);
        assert(modify->total_ns == 3000);

        assert(profiler.total_profiled_ns() == 6000);
    }

    void test_min_max_average()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("Matching", 100);
        profiler.record("Matching", 300);
        profiler.record("Matching", 500);
        profiler.record("Matching", 700);

        const auto results = profiler.results();

        assert(results.size() == 1);

        const ProfileResult *result =
            find_result(results, "Matching");

        assert(result != nullptr);

        assert(result->calls == 4);
        assert(result->total_ns == 1600);
        assert(result->min_ns == 100);
        assert(result->max_ns == 700);
        assert(result->avg_ns == 400);

        assert(profiler.total_profiled_ns() == 1600);
    }

    void test_sorting_by_total_time()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("FastOperation", 100);
        profiler.record("SlowOperation", 5000);
        profiler.record("MediumOperation", 1000);

        const auto results = profiler.results();

        assert(results.size() == 3);

        assert(results[0].name == "SlowOperation");
        assert(results[1].name == "MediumOperation");
        assert(results[2].name == "FastOperation");

        assert(results[0].total_ns == 5000);
        assert(results[1].total_ns == 1000);
        assert(results[2].total_ns == 100);
    }

    void test_deterministic_tie_breaking()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("Beta", 1000);
        profiler.record("Gamma", 1000);
        profiler.record("Alpha", 1000);

        const auto results = profiler.results();

        assert(results.size() == 3);

        assert(results[0].name == "Alpha");
        assert(results[1].name == "Beta");
        assert(results[2].name == "Gamma");
    }

    void test_share_percentages()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("Small", 1000);
        profiler.record("Large", 3000);

        const auto results = profiler.results();

        assert(results.size() == 2);

        const ProfileResult *small =
            find_result(results, "Small");

        const ProfileResult *large =
            find_result(results, "Large");

        assert(small != nullptr);
        assert(large != nullptr);

        assert(std::abs(small->share_percent - 25.0) < 1e-9);
        assert(std::abs(large->share_percent - 75.0) < 1e-9);

        const double share_sum =
            small->share_percent + large->share_percent;

        assert(std::abs(share_sum - 100.0) < 1e-9);
    }

    void test_zero_duration()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("ZeroDuration", 0);

        const auto results = profiler.results();

        assert(results.size() == 1);

        const ProfileResult *result =
            find_result(results, "ZeroDuration");

        assert(result != nullptr);

        assert(result->calls == 1);
        assert(result->total_ns == 0);
        assert(result->min_ns == 0);
        assert(result->max_ns == 0);
        assert(result->avg_ns == 0);
        assert(result->share_percent == 0.0);

        assert(profiler.total_profiled_ns() == 0);
    }

    void test_profile_scope_records_measurement()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        {
            ProfileScope scope(profiler, "ProfileScopeTest");
        }

        const auto results = profiler.results();

        assert(results.size() == 1);

        const ProfileResult *result =
            find_result(results, "ProfileScopeTest");

        assert(result != nullptr);
        assert(result->calls == 1);

        // The exact duration is platform-dependent.
        assert(result->total_ns == result->min_ns);
        assert(result->total_ns == result->max_ns);
        assert(result->total_ns == result->avg_ns);

        assert(profiler.total_profiled_ns() == result->total_ns);
    }

    void test_profile_scope_aggregation()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        {
            ProfileScope scope(profiler, "RepeatedScope");
        }

        {
            ProfileScope scope(profiler, "RepeatedScope");
        }

        {
            ProfileScope scope(profiler, "RepeatedScope");
        }

        const auto results = profiler.results();

        assert(results.size() == 1);

        const ProfileResult *result =
            find_result(results, "RepeatedScope");

        assert(result != nullptr);

        assert(result->calls == 3);
        assert(result->total_ns >= result->min_ns);
        assert(result->total_ns >= result->max_ns);
        assert(result->avg_ns <= result->max_ns);
        assert(result->avg_ns >= result->min_ns);

        assert(profiler.total_profiled_ns() == result->total_ns);
    }

    void test_nested_profile_scopes()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        {
            ProfileScope outer(profiler, "Outer");

            {
                ProfileScope inner(profiler, "Inner");
            }
        }

        const auto results = profiler.results();

        assert(results.size() == 2);

        const ProfileResult *outer =
            find_result(results, "Outer");

        const ProfileResult *inner =
            find_result(results, "Inner");

        assert(outer != nullptr);
        assert(inner != nullptr);

        assert(outer->calls == 1);
        assert(inner->calls == 1);

        assert(outer->total_ns >= inner->total_ns);
    }

    void test_total_profiled_time()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("A", 1500);
        profiler.record("B", 2500);
        profiler.record("C", 4000);

        assert(profiler.total_profiled_ns() == 8000);
    }

    void test_reset_clears_all_data()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("OrderBook::add", 1000);
        profiler.record("Matching", 2000);

        assert(!profiler.results().empty());
        assert(profiler.total_profiled_ns() == 3000);

        profiler.reset();

        assert(profiler.results().empty());
        assert(profiler.total_profiled_ns() == 0);
    }

    void test_report_contains_header()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("OrderBook::add", 1000);

        std::ostringstream output;

        profiler.print_report(output);

        const std::string report = output.str();

        assert(report.find("PROFILE REPORT") != std::string::npos);
        assert(report.find("Scope") != std::string::npos);
        assert(report.find("Calls") != std::string::npos);
        assert(report.find("Total(ns)") != std::string::npos);
        assert(report.find("Avg(ns)") != std::string::npos);
        assert(report.find("Min(ns)") != std::string::npos);
        assert(report.find("Max(ns)") != std::string::npos);
        assert(report.find("Share") != std::string::npos);
        assert(report.find("Total profiled time:") != std::string::npos);
    }

    void test_report_contains_scopes()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("OrderBook::add", 1000);
        profiler.record("OrderBook::cancel", 2000);
        profiler.record("OrderBook::modify", 3000);

        std::ostringstream output;

        profiler.print_report(output);

        const std::string report = output.str();

        assert(report.find("OrderBook::add") != std::string::npos);
        assert(report.find("OrderBook::cancel") != std::string::npos);
        assert(report.find("OrderBook::modify") != std::string::npos);

        assert(
            report.find("Total profiled time: 6000 ns") !=
            std::string::npos);
    }

    void test_report_order_matches_results()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("First", 1000);
        profiler.record("Second", 5000);
        profiler.record("Third", 3000);

        const auto results = profiler.results();

        std::ostringstream output;

        profiler.print_report(output);

        const std::string report = output.str();

        assert(results.size() == 3);

        const std::size_t second_pos =
            report.find("Second");

        const std::size_t third_pos =
            report.find("Third");

        const std::size_t first_pos =
            report.find("First");

        assert(second_pos != std::string::npos);
        assert(third_pos != std::string::npos);
        assert(first_pos != std::string::npos);

        assert(second_pos < third_pos);
        assert(third_pos < first_pos);
    }

    void test_report_row_limit()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("ROW_ONE", 1000);
        profiler.record("ROW_TWO", 2000);
        profiler.record("ROW_THREE", 3000);

        std::ostringstream output;

        profiler.print_report(output, 2);

        const std::string report = output.str();

        // Results are sorted by total time:
        // ROW_THREE, ROW_TWO, ROW_ONE.
        assert(report.find("ROW_THREE") != std::string::npos);
        assert(report.find("ROW_TWO") != std::string::npos);
        assert(report.find("ROW_ONE") == std::string::npos);
    }

    void test_report_zero_row_limit_means_all()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        profiler.record("ROW_A", 1000);
        profiler.record("ROW_B", 2000);
        profiler.record("ROW_C", 3000);

        std::ostringstream output;

        profiler.print_report(output, 0);

        const std::string report = output.str();

        assert(report.find("ROW_A") != std::string::npos);
        assert(report.find("ROW_B") != std::string::npos);
        assert(report.find("ROW_C") != std::string::npos);
    }

    void test_report_with_empty_profiler()
    {
        Profiler &profiler = Profiler::instance();

        profiler.reset();

        std::ostringstream output;

        profiler.print_report(output);

        const std::string report = output.str();

        assert(report.find("PROFILE REPORT") != std::string::npos);
        assert(report.find("Total profiled time: 0 ns") !=
               std::string::npos);
    }

} // namespace

int main()
{
    test_empty_profiler();

    test_single_record();
    test_multiple_records_same_scope();
    test_multiple_scopes();

    test_min_max_average();
    test_sorting_by_total_time();
    test_deterministic_tie_breaking();
    test_share_percentages();

    test_zero_duration();

    test_profile_scope_records_measurement();
    test_profile_scope_aggregation();
    test_nested_profile_scopes();

    test_total_profiled_time();
    test_reset_clears_all_data();

    test_report_contains_header();
    test_report_contains_scopes();
    test_report_order_matches_results();
    test_report_row_limit();
    test_report_zero_row_limit_means_all();
    test_report_with_empty_profiler();

    std::cout << "All Profiling Tests Passed";

    return 0;
}