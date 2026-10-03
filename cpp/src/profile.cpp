#include <profile.hpp>

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <utility>

Profiler &Profiler::instance()
{
    static Profiler profiler;
    return profiler;
}

void Profiler::record(
    std::string_view name,
    std::uint64_t duration_ns)
{
    auto &entry = entries_[std::string(name)];

    ++entry.calls;
    entry.total_ns += duration_ns;

    if (entry.calls == 1)
    {
        entry.min_ns = duration_ns;
        entry.max_ns = duration_ns;
    }
    else
    {
        entry.min_ns = std::min(entry.min_ns, duration_ns);
        entry.max_ns = std::max(entry.max_ns, duration_ns);
    }

    total_profiled_ns_ += duration_ns;
}

void Profiler::reset()
{
    entries_.clear();
    total_profiled_ns_ = 0;
}

std::vector<ProfileResult> Profiler::results() const
{
    std::vector<ProfileResult> results;
    results.reserve(entries_.size());

    for (const auto &[name, entry] : entries_)
    {
        ProfileResult result;

        result.name = name;
        result.calls = entry.calls;
        result.total_ns = entry.total_ns;
        result.min_ns = entry.min_ns;
        result.max_ns = entry.max_ns;

        if (entry.calls != 0)
        {
            result.avg_ns = entry.total_ns / entry.calls;
        }

        if (total_profiled_ns_ != 0)
        {
            result.share_percent =
                100.0 *
                static_cast<double>(entry.total_ns) /
                static_cast<double>(total_profiled_ns_);
        }

        results.push_back(std::move(result));
    }

    std::sort(
        results.begin(),
        results.end(),
        [](const ProfileResult &lhs, const ProfileResult &rhs)
        {
            if (lhs.total_ns != rhs.total_ns)
            {
                return lhs.total_ns > rhs.total_ns;
            }

            return lhs.name < rhs.name;
        });

    return results;
}

std::uint64_t Profiler::total_profiled_ns() const noexcept
{
    return total_profiled_ns_;
}

void Profiler::print_report(
    std::ostream &out,
    std::size_t max_rows) const
{
    const auto data = results();

    out << '\n';
    out << "============================================================\n";
    out << "PROFILE REPORT\n";
    out << "============================================================\n\n";

    out << std::left
        << std::setw(36) << "Scope"
        << std::right
        << std::setw(12) << "Calls"
        << std::setw(16) << "Total(ns)"
        << std::setw(16) << "Avg(ns)"
        << std::setw(16) << "Min(ns)"
        << std::setw(16) << "Max(ns)"
        << std::setw(12) << "Share"
        << '\n';

    out << std::string(124, '-') << '\n';

    std::size_t rows_printed = 0;

    for (const auto &result : data)
    {
        if (max_rows != 0 && rows_printed >= max_rows)
        {
            break;
        }

        out << std::left
            << std::setw(36) << result.name
            << std::right
            << std::setw(12) << result.calls
            << std::setw(16) << result.total_ns
            << std::setw(16) << result.avg_ns
            << std::setw(16) << result.min_ns
            << std::setw(16) << result.max_ns
            << std::setw(11)
            << std::fixed
            << std::setprecision(2)
            << result.share_percent
            << '%'
            << '\n';

        ++rows_printed;
    }

    out << '\n';
    out << "Total profiled time: "
        << total_profiled_ns_
        << " ns\n";
}

ProfileScope::ProfileScope(
    Profiler &profiler,
    std::string_view name)
    : profiler_(&profiler),
      name_(name),
      start_(Profiler::Clock::now())
{
}

ProfileScope::~ProfileScope()
{
    const auto end = Profiler::Clock::now();

    const auto duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start_);

    try
    {
        profiler_->record(
            name_,
            static_cast<std::uint64_t>(duration.count()));
    }
    catch (...)
    {
    }
}