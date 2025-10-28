#ifndef OVERSETSIMULATION_H
#define OVERSETSIMULATION_H
#include "mpi.h"
#include "tioga.h"
#include "ExawindSolver.h"
#include "ParallelPrinter.h"
#include "Timers.h"

namespace TIOGA {
class tioga;
}

namespace exawind {

class OversetSimulation
{
private:
    //! World communicator instance
    MPI_Comm m_comm;
    //! List of solvers active in this overset simulation
    std::vector<std::unique_ptr<ExawindSolver>> m_solvers;
    //! List of start ranks for all nalu-wind instances
    int m_num_nw_solvers;
    std::vector<int> m_nw_start_rank;
    //! Flag indicating whether an AMR solver is active
    bool m_has_amr{false};
    //! Flag indicating whether an unstructured solver is active
    bool m_has_unstructured{false};
    //! Interval for overset updates during timestepping
    int m_overset_update_interval{100000000};
    //! Last timestep run during this simulation
    int m_last_timestep{0};
    //! Flag indicating whether all solvers use fixed dt. If any solver uses
    //! adaptive dt, then this flag will be false
    bool m_fixed_dt{true};
    //! Flag indicating whether initialization tasks have been performed
    bool m_initialized{false};
    //! Flag indicating if complementary comms have been initialized
    bool m_complementary_comm_initialized{false};
    //! Flag for holemap algorithm
    bool m_is_adaptive_holemap_alg{false};
    //! Number of composite bodies
    int m_num_composite_bodies{0};
    //! Tioga instance
    TIOGA::tioga m_tg;
    //! Determine unstructured and structured solver types
    void check_solver_types();
    //! Determine if we should update connectivity during time integration
    void determine_overset_interval();
    //! Return True if connectivity must be updated at a given timestep
    bool do_connectivity(const int tstep);
    //! Parallel printer utility
    ParallelPrinter m_printer;
    //! Timer names
    const std::vector<std::string> m_names_exa{"TimeStep"};
    const std::vector<std::string> m_names_tg{"Connectivity", "SolExchange"};
    //! Timer
    Timers m_timers_exa;
    Timers m_timers_tg;

public:
    OversetSimulation(MPI_Comm comm);
    ~OversetSimulation();

    //! Register a solver
    template <class Solver, class... Args>
    void register_solver(Args... args)
    {
        m_solvers.emplace_back(
            std::make_unique<Solver>(std::forward<Args>(args)..., m_tg));
    }

    //! Delete solvers
    void delete_solvers()
    {
        for (auto& ss : m_solvers) {
            ss.reset();
        }
    }

    //! Initialize all solvers
    void initialize();

    //! Determine field, fringe, hole information
    void perform_overset_connectivity();

    //! Exchange solution between solvers
    void exchange_solution(const bool increment_time = false);

    //! Run prescribed number of timesteps
    void run_timesteps(
        const int add_pic_its,
        const int nonlinear_its = 1,
        const int nsteps = 1,
        const double max_time = -1.);

    //! Print something
    void echo(const std::string& out) { m_printer.echo(out); }

    //! track detailed timing and print to file
    void print_timing(const int nt);

    //! track memory usage and print to file
    long mem_usage_all(const int step);

    //! set number of nalu-wind instances
    void set_nw_start_rank(const std::vector<int>& start_ranks)
    {
        if (!start_ranks.empty()) {
            m_nw_start_rank = start_ranks;
        } else {
            m_nw_start_rank.clear();
        }
        m_num_nw_solvers = m_nw_start_rank.size();
    }

    void set_holemap_alg(bool alg)
    {
        m_is_adaptive_holemap_alg = alg;
        if (m_is_adaptive_holemap_alg) m_tg.setHoleMapAlgorithm(1);
    }

    void set_composite_num(const int num_composite)
    {
        m_num_composite_bodies = num_composite;
        m_tg.setNumCompositeBodies(num_composite);
    }

    void set_composite_body(
        int body_index,
        const int num_body_tags,
        std::vector<int> bodytags,
        std::vector<int> dominance_tags,
        const double search_tol)
    {
        if (m_is_adaptive_holemap_alg) {
            m_tg.registerCompositeBody(
                (body_index + 1), num_body_tags, bodytags.data(),
                dominance_tags.data(), search_tol);
        } else {
            throw std::runtime_error(
                "The composite body feature requires the use of adaptive "
                "holemap");
        }
    }
};

} // namespace exawind
#endif /* OVERSETSIMULATION_H */
