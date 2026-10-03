#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct ProfileResult
{
    std::string name;

    std::uint64_t calls = 0;
    std::uint64_t total_ns = 0;
    std::uint64_t min_ns = 0;
    std::uint64_t max_ns = 0;
    std::uint64_t avg_ns = 0;

    double share_percent = 0.0;
};

class Profiler
{
public:
    using Clock = std::chrono::steady_clock;

    static Profiler &instance();

    void record(std::string_view name, std::uint64_t duration_ns);

    void reset();

    [[nodiscard]] std::vector<ProfileResult> results() const;

    [[nodiscard]] std::uint64_t total_profiled_ns() const noexcept;

    void print_report(
        std::ostream &out,
        std::size_t max_rows = 0) const;

private:
    struct Entry
    {
        std::uint64_t calls = 0;
        std::uint64_t total_ns = 0;
        std::uint64_t min_ns = 0;
        std::uint64_t max_ns = 0;
    };

    std::unordered_map<std::string, Entry> entries_;
    std::uint64_t total_profiled_ns_ = 0;
};

class ProfileScope
{
public:
    ProfileScope(
        Profiler &profiler,
        std::string_view name);

    ~ProfileScope();

    ProfileScope(const ProfileScope &) = delete;
    ProfileScope &operator=(const ProfileScope &) = delete;

    ProfileScope(ProfileScope &&) = delete;
    ProfileScope &operator=(ProfileScope &&) = delete;

private:
    Profiler *profiler_;
    std::string name_;
    Profiler::Clock::time_point start_;
};

#define LOB_PROFILE_SCOPE(name) \
    ProfileScope profile_scope_##__LINE__(Profiler::instance(), name)