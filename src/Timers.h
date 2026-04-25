#ifndef TIMERS_H
#define TIMERS_H

#include "mpi.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <string>

namespace driver {

class Timer
{
    using ClockT = std::chrono::steady_clock;
    using TimePt = std::chrono::time_point<ClockT>;
    using TimeT = std::chrono::milliseconds;

    TimePt _start = ClockT::now(), _end = {};
    TimeT _increment;

public:
    void tick(const bool incremental)
    {
        _start = ClockT::now();
        if ((incremental) && (_end != TimePt{})) {
            _start -= _increment;
        }
        _end = TimePt{};
        _increment = duration();
    }

    void tock()
    {
        _end = ClockT::now();
        _increment = duration();
    }

    TimeT duration() const
    {
        return std::chrono::duration_cast<TimeT>(_end - _start);
    }
};

struct Timers
{
    std::vector<Timer> m_timers;
    std::vector<std::string> m_names;

    Timers(const std::vector<std::string>& names) : m_names(names)
    {
        Timer clock;
        for ([[maybe_unused]] const auto& name : names) {
            m_timers.push_back(clock);
        }
    };

    void addTimer(std::string name)
    {
        m_names.push_back(name);
        Timer clock;
        m_timers.push_back(clock);
    }

    const std::vector<double> counts()
    {
        std::vector<double> counts;
        for (auto const& timer : m_timers) {
            counts.push_back(timer.duration().count());
        }
        return counts;
    };

    void tick(const std::string name, const bool incremental = false)
    {
        m_timers.at(idx(name)).tick(incremental);
    };

    void tock(const std::string name) { m_timers.at(idx(name)).tock(); };

    int idx(const std::string key)
    {
        std::vector<std::string>::iterator itr =
            std::find(m_names.begin(), m_names.end(), key);

        assert(itr != m_names.cend());
        return std::distance(m_names.begin(), itr);
    }

    std::string get_timings_summary(
        std::string solver, int step, MPI_Comm comm, int root = 0)
    {
        double total_min, total_avg, total_max;
        total_times(total_min, total_avg, total_max, comm, root);

        std::ostringstream total_time_out = get_line_output(
            solver, step, "Total", total_min, total_avg, total_max);
        return total_time_out.str();
    };

    std::string get_timings_detail(
        std::string solver, int step, MPI_Comm comm, int root = 0)
    {
        std::vector<double> mintimes(m_timers.size(), 0.0);
        std::vector<double> avgtimes(m_timers.size(), 0.0);
        std::vector<double> maxtimes(m_timers.size(), 0.0);
        par_reduce_times(mintimes, avgtimes, maxtimes, comm, root);

        std::ostringstream outstream;
        std::ostringstream linestream;
        for (int i = 0; i < static_cast<int>(m_timers.size()); ++i) {
            std::string func_call =
                (m_timers.size() == 1) ? "Total" : m_names.at(i);

            linestream = get_line_output(
                solver, step, func_call, mintimes.at(i), avgtimes.at(i),
                maxtimes.at(i));

            outstream << linestream.str();
            if (i < static_cast<int>(m_timers.size()) - 1)
                outstream << std::endl;
        }

        //  accumulate only if there is more than 1 routine to report
        if (m_timers.size() > 1) {
            double total_min, total_avg, total_max;
            total_times(total_min, total_avg, total_max, comm, root);

            outstream << std::endl;
            linestream = get_line_output(
                solver, step, "Total", total_min, total_avg, total_max);
            outstream << linestream.str();
        }

        return outstream.str();
    };

    void total_times(
        double& total_min,
        double& total_avg,
        double& total_max,
        MPI_Comm comm,
        int root)
    {
        std::vector<double> mintimes(m_timers.size(), 0.0);
        std::vector<double> avgtimes(m_timers.size(), 0.0);
        std::vector<double> maxtimes(m_timers.size(), 0.0);
        par_reduce_times(mintimes, avgtimes, maxtimes, comm, root);

        total_min = std::accumulate(mintimes.begin(), mintimes.end(), 0.0);
        total_avg = std::accumulate(mintimes.begin(), mintimes.end(), 0.0);
        total_max = std::accumulate(maxtimes.begin(), maxtimes.end(), 0.0);
    }

    void par_reduce_times(
        std::vector<double>& mintimes,
        std::vector<double>& avgtimes,
        std::vector<double>& maxtimes,
        MPI_Comm comm,
        int root)
    {
        const auto times = counts();
        MPI_Reduce(
            times.data(), mintimes.data(), m_timers.size(), MPI_DOUBLE, MPI_MIN,
            root, comm);
        MPI_Reduce(
            times.data(), avgtimes.data(), m_timers.size(), MPI_DOUBLE, MPI_SUM,
            root, comm);
        MPI_Reduce(
            times.data(), maxtimes.data(), m_timers.size(), MPI_DOUBLE, MPI_MAX,
            root, comm);

        int psize;
        MPI_Comm_size(comm, &psize);
        for (auto& elem : avgtimes) {
            elem /= psize;
        }
    }

    std::ostringstream get_line_output(
        std::string solver,
        int step,
        std::string func_call,
        double min,
        double avg,
        double max)
    {
        std::ostringstream outstream;
        const double ms2s = 1000.0;
        const char separator = ' ';
        const int name_width = 25;
        const int num_width = 10;
        const int num_precision = 4;

        outstream << std::left << std::setw(name_width)
                  << std::setfill(separator) << solver + "::" + func_call
                  << std::setw(num_width) << std::setfill(separator)
                  << std::fixed << std::setprecision(num_precision)
                  << std::right << step << std::setw(num_width)
                  << std::setfill(separator) << std::fixed
                  << std::setprecision(num_precision) << std::right
                  << (min / ms2s) << std::setw(num_width)
                  << std::setfill(separator) << std::fixed
                  << std::setprecision(num_precision) << std::right
                  << (avg / ms2s) << std::setw(num_width)
                  << std::setfill(separator) << std::fixed
                  << std::setprecision(num_precision) << std::right
                  << (max / ms2s);
        return outstream;
    }
};
} // namespace driver
#endif /* TIMERS_H */
