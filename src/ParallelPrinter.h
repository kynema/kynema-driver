#ifndef PARALLELPRINTER_H
#define PARALLELPRINTER_H
#include <fstream>
#include <iostream>
#include <ostream>
#include "mpi.h"

namespace driver {

class ParallelPrinter
{
private:
    MPI_Comm m_comm;
    int m_rank;
    int m_io_rank;

public:
    ParallelPrinter(MPI_Comm comm, const int io_rank = 0)
        : m_comm(comm), m_io_rank(io_rank)
    {
        MPI_Comm_rank(m_comm, &m_rank);
    };

    void echo(const std::string& out)
    {
        if (m_rank == m_io_rank) std::cout << out << std::endl;
    };

    void reset()
    {
        const std::string filename = "timings.dat";
        remove(filename.c_str());

        std::ofstream fp;
        fp.open(filename.c_str(), std::ios_base::out);

        timing_to_file(time_header());

        fp.close();
    };

    void timing_to_file(const std::string& out)
    {
        if (m_rank == m_io_rank) {
            const std::string filename = "timings.dat";
            std::ofstream fp;
            fp.open(filename.c_str(), std::ios_base::app);
            fp << out << std::endl;
            fp.close();
        }
    };

    void echo_time_header()
    {
        std::string header = time_header();

        std::string hyphens;
        hyphens.assign(65, '-');

        std::ostringstream out;
        out << std::endl
            << hyphens << std::endl
            << header << std::endl
            << hyphens;

        echo(out.str());
    };

    std::string time_header()
    {
        std::ostringstream outstream;
        const char separator = ' ';
        const int name_width = 25;
        const int num_width = 10;
        const int num_precision = 4;

        outstream << std::left << std::setw(name_width)
                  << std::setfill(separator) << "Routine"
                  << std::setw(num_width) << std::setfill(separator)
                  << std::fixed << std::setprecision(num_precision)
                  << std::right << "Step" << std::setw(num_width)
                  << std::setfill(separator) << std::fixed
                  << std::setprecision(num_precision) << std::right << "Min"
                  << std::setw(num_width) << std::setfill(separator)
                  << std::fixed << std::setprecision(num_precision)
                  << std::right << "Avg" << std::setw(num_width)
                  << std::setfill(separator) << std::fixed
                  << std::setprecision(num_precision) << std::right << "Max";
        return outstream.str();
    };

    int io_rank() { return m_io_rank; };
    bool is_io_rank() { return m_rank == m_io_rank; };
};
} // namespace driver
#endif /* PARALLELPRINTER_H */
