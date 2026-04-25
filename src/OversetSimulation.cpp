#include "OversetSimulation.h"
#include "MemoryUsage.h"
#include "Timers.h"
#include <algorithm>
#include <fstream>

namespace driver {

OversetSimulation::OversetSimulation(MPI_Comm comm)
    : m_comm(comm)
    , m_printer(comm)
    , m_timers_exa(m_names_exa)
    , m_timers_tg(m_names_tg)
{
    int psize, prank;
    MPI_Comm_size(m_comm, &psize);
    MPI_Comm_rank(m_comm, &prank);
    m_tg.setCommunicator(MPI_COMM_WORLD, prank, psize);
    m_printer.reset();
}

OversetSimulation::~OversetSimulation() = default;

void OversetSimulation::check_solver_types()
{
    bool flag[2] = {false, false};
    bool gflag[2] = {false, false};
    flag[0] =
        std::any_of(m_solvers.begin(), m_solvers.end(), [](const auto& ss) {
            return ss->is_amr();
        });
    flag[1] =
        std::any_of(m_solvers.begin(), m_solvers.end(), [](const auto& ss) {
            return ss->is_unstructured();
        });
    MPI_Allreduce(flag, gflag, 2, MPI_C_BOOL, MPI_LOR, m_comm);
    m_has_amr = gflag[0];
    m_has_unstructured = gflag[1];
}

void OversetSimulation::determine_overset_interval()
{
    int flag[1] = {m_overset_update_interval};
    int gflag[1] = {m_overset_update_interval};
    for (auto& ss : m_solvers) {
        flag[0] = std::min(flag[0], ss->overset_update_interval());
    }
    MPI_Allreduce(flag, gflag, 1, MPI_INT, MPI_MIN, m_comm);
    m_overset_update_interval = gflag[0];
    m_printer.echo(
        "Overset update interval = " +
        std::to_string(m_overset_update_interval));
}

void OversetSimulation::initialize()
{
    check_solver_types();
    if (!m_has_unstructured) {
        throw std::runtime_error(
            "OversetSimulationulation requires at least one unstructured "
            "solver");
    }

    for (auto& ss : m_solvers) ss->call_init_prolog(true);

    determine_overset_interval();

    for (auto& ss : m_solvers) {
        ss->call_init_epilog();
        ss->call_prepare_solver_prolog();
    }

    perform_overset_connectivity();
    exchange_solution();

    for (auto& ss : m_solvers) ss->call_prepare_solver_epilog();

    if (!(std::all_of(m_solvers.begin() + 1, m_solvers.end(), [&](auto& ss) {
            return ss->time_index() == m_solvers.at(0)->time_index();
        }))) {
        throw std::runtime_error("Mismatch in solver time steps.");
    }
    m_last_timestep = m_solvers.at(0)->time_index();

    // Determine if any of the solvers have adaptive timestepping
    for (auto& ss : m_solvers) {
        m_fixed_dt = m_fixed_dt && ss->is_fixed_timestep_size();
    }
    MPI_Allreduce(MPI_IN_PLACE, &m_fixed_dt, 1, MPI_C_BOOL, MPI_LAND, m_comm);

    m_initialized = true;
}

void OversetSimulation::perform_overset_connectivity()
{
    for (auto& ss : m_solvers) ss->call_pre_overset_conn_work();

    m_timers_tg.tick("Connectivity");
    if (m_has_amr) m_tg.preprocess_amr_data();
    m_tg.profile();
    if ((m_is_adaptive_holemap_alg == 1) &&
        (m_complementary_comm_initialized == false)) {
        m_tg.assembleComplementComms();
        m_complementary_comm_initialized = true;
        if (m_num_composite_bodies > 0) m_tg.assembleCompositeMap();
    }
    m_tg.performConnectivity();
    if (m_has_amr) m_tg.performConnectivityAMR();
    m_timers_tg.tock("Connectivity");

    for (auto& ss : m_solvers) ss->call_post_overset_conn_work();
}

void OversetSimulation::exchange_solution(bool increment_time)
{

    for (auto& ss : m_solvers) ss->call_register_solution();

    m_timers_tg.tick("SolExchange", increment_time);
    if (m_has_amr) {
        m_tg.dataUpdate_AMR();
    } else {
        const int row_major = 0;
        // assuming this pathway is kynema-ugf only and all instances have same
        // number of field components
        const int ncomps = m_solvers[0]->get_ncomps();
        m_tg.dataUpdate(ncomps, row_major);
    }
    m_timers_tg.tock("SolExchange");

    for (auto& ss : m_solvers) ss->call_update_solution();
}

void OversetSimulation::run_timesteps(
    const int add_pic_its,
    const int nonlinear_its,
    const int nsteps,
    const double max_time)
{

    if (!m_initialized) {
        throw std::runtime_error("OversetSimulation has not been initialized");
    }

    const int tstart = m_last_timestep + 1;
    const int tend = m_last_timestep + 1 + nsteps;
    m_printer.echo(
        "Running " + std::to_string(nsteps) + " timesteps starting from " +
        std::to_string(tstart));

    int nt = tstart;
    double time = max_time < 0. ? 0. : m_solvers[0]->call_get_time();
    bool step_check = nsteps > 0 ? nt < tend : true;
    bool time_check = max_time > 0. ? time < max_time : true;
    bool do_step = step_check && time_check;
    while (do_step) {
        m_printer.echo_time_header();

        m_timers_exa.tick("TimeStep");

        for (size_t inonlin = 0; inonlin < static_cast<size_t>(nonlinear_its);
             inonlin++) {

            bool increment_timer = inonlin > 0 ? true : false;

            double dt{1e8};
            for (auto& ss : m_solvers) {
                ss->call_pre_advance_stage0(inonlin, increment_timer);
                if (inonlin < 1 && !m_fixed_dt) {
                    dt = std::min(dt, ss->call_get_timestep_size());
                }
            }

            if (inonlin < 1 && !m_fixed_dt) {
                MPI_Allreduce(
                    MPI_IN_PLACE, &dt, 1, MPI_DOUBLE, MPI_MIN, m_comm);
                for (auto& ss : m_solvers) ss->call_set_timestep_size(dt);
            }

            for (auto& ss : m_solvers)
                ss->call_pre_advance_stage1(inonlin, increment_timer);

            if (do_connectivity(nt)) perform_overset_connectivity();

            for (auto& ss : m_solvers)
                ss->call_pre_advance_stage2(inonlin, increment_timer);

            exchange_solution(increment_timer);

            for (auto& ss : m_solvers)
                ss->call_advance_timestep(inonlin, increment_timer);
        }

        if (add_pic_its > 0) {
            exchange_solution(true);
            for (auto& ss : m_solvers)
                ss->call_additional_picard_iterations(add_pic_its);
        }

        for (auto& ss : m_solvers) ss->call_post_advance();

        MPI_Barrier(m_comm);

        m_timers_exa.tock("TimeStep");

        MPI_Barrier(m_comm);

        print_timing(nt);

        MPI_Barrier(m_comm);

        mem_usage_all(nt);

        ++nt;
        if (max_time > 0.) time = m_solvers[0]->call_get_time();
        step_check = nsteps > 0 ? nt < tend : true;
        time_check = max_time > 0. ? time < max_time : true;
        do_step = step_check && time_check;
    }
    for (auto& ss : m_solvers) ss->call_dump_simulation_time();
    m_last_timestep = tend;
}

bool OversetSimulation::do_connectivity(const int tstep)
{
    return (tstep > 0) && (tstep % m_overset_update_interval) == 0;
}

void OversetSimulation::print_timing(const int nt)
{
    // overall timestep timing
    auto timing_summary = m_timers_exa.get_timings_summary(
        "Exawind", nt, m_comm, m_printer.io_rank());
    m_printer.echo(timing_summary);

    auto timing_detail = m_timers_exa.get_timings_detail(
        "Exawind", nt, m_comm, m_printer.io_rank());
    m_printer.timing_to_file(timing_detail);

    MPI_Barrier(m_comm);

    // tioga timing
    timing_summary = m_timers_tg.get_timings_summary(
        "Tioga", nt, m_comm, m_printer.io_rank());
    m_printer.echo(timing_summary);

    timing_detail = m_timers_tg.get_timings_detail(
        "Tioga", nt, m_comm, m_printer.io_rank());
    m_printer.timing_to_file(timing_detail);

    // cfd solver-specific timing
    for (auto& ss : m_solvers) {
        MPI_Barrier(m_comm);

        ParallelPrinter printer(ss->comm());

        // summary timings
        std::string timings_summary = ss->m_timers.get_timings_summary(
            ss->identifier(), nt, ss->comm(), printer.io_rank());
        printer.echo(timings_summary);

        // detailed timings
        std::string timings_detail = ss->m_timers.get_timings_detail(
            ss->identifier(), nt, ss->comm(), printer.io_rank());
        printer.timing_to_file(timings_detail);
    }

    MPI_Barrier(m_comm);
}

long OversetSimulation::mem_usage_all(const int step)
{
    const long mem = memory_usage();

    int psize;
    MPI_Comm_size(m_comm, &psize);

    // gather all memory usage
    std::vector<long> memall(psize);
    MPI_Gather(
        &mem, 1, MPI_LONG, memall.data(), 1, MPI_LONG, m_printer.io_rank(),
        m_comm);

    // FIXME: move to separate output files and put in ExawindSolver
    if (m_printer.is_io_rank()) {
        const std::string filename = "memusage.dat";
        std::ofstream fp;

        if (step == m_last_timestep + 1) {
            fp.open(filename.c_str(), std::ios_base::out);
            fp << "# time step, memory usage in MBs" << std::endl;
        } else {
            fp.open(filename.c_str(), std::ios_base::app);
        }

        fp << std::to_string(step);
        for (const auto& m : memall) {
            fp << ' ' << m;
        }
        fp << std::endl;
        fp.close();
    }
    return mem;
}

} // namespace driver
