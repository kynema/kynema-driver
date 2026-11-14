#include "Simulation.h"
#include "MemoryUsage.h"
#include "Timers.h"
#include <algorithm>
#include <fstream>
#include <limits>

namespace driver {

Simulation::Simulation(MPI_Comm comm)
    : m_comm(comm)
    , m_printer(comm)
    , m_timers_kyn(m_names_exa)
    , m_timers_tg(m_names_tg)
{
    int psize, prank;
    MPI_Comm_size(m_comm, &psize);
    MPI_Comm_rank(m_comm, &prank);
    m_tg.setCommunicator(m_comm, prank, psize);
    m_printer.reset();
}

Simulation::~Simulation() = default;

void Simulation::check_module_types()
{
    bool flag[2] = {false, false};
    bool gflag[2] = {false, false};
    flag[0] =
        std::any_of(m_modules.begin(), m_modules.end(), [](const auto& module) {
            return module->is_cfd() && module->is_amr();
        });
    flag[1] =
        std::any_of(m_modules.begin(), m_modules.end(), [](const auto& module) {
            return module->is_cfd() && module->is_unstructured();
        });
    MPI_Allreduce(flag, gflag, 2, MPI_C_BOOL, MPI_LOR, m_comm);
    m_has_amr = gflag[0];
    m_has_unstructured = gflag[1];
}

void Simulation::determine_overset_interval()
{
    if (!m_use_overset) return;
    int flag[1] = {m_overset_update_interval};
    int gflag[1] = {m_overset_update_interval};
    for (auto& module : m_modules) {
        if (module->is_cfd())
            flag[0] = std::min(flag[0], module->overset_update_interval());
    }
    MPI_Allreduce(flag, gflag, 1, MPI_INT, MPI_MIN, m_comm);
    m_overset_update_interval = gflag[0];
    m_printer.echo(
        "Overset update interval = " +
        std::to_string(m_overset_update_interval));
}

void Simulation::initialize()
{
    check_module_types();
    int local_modules = 0;
    int local_cfd_modules = 0;
    for (auto& module : m_modules) {
        int rank = 0;
        MPI_Comm_rank(module->comm(), &rank);
        if (rank == 0) {
            ++local_modules;
            if (module->is_cfd()) ++local_cfd_modules;
        }
    }
    int global_modules = 0, global_cfd_modules = 0;
    MPI_Allreduce(&local_modules, &global_modules, 1, MPI_INT, MPI_SUM, m_comm);
    MPI_Allreduce(&local_cfd_modules, &global_cfd_modules, 1, MPI_INT, MPI_SUM, m_comm);
    if (global_modules == 0) throw std::runtime_error("No modules registered");
    m_use_overset = global_cfd_modules > 1;

    for (auto& module : m_modules) module->call_init_prolog(m_use_overset);

    determine_overset_interval();

    for (auto& module : m_modules) {
        module->call_init_epilog();
        module->call_prepare_module_prolog();
    }

    perform_overset_connectivity();
    exchange_solution();

    for (auto& module : m_modules) module->call_prepare_module_epilog();

    int min_step = std::numeric_limits<int>::max();
    int max_step = std::numeric_limits<int>::min();
    for (auto& module : m_modules) {
        min_step = std::min(min_step, module->time_index());
        max_step = std::max(max_step, module->time_index());
    }
    MPI_Allreduce(MPI_IN_PLACE, &min_step, 1, MPI_INT, MPI_MIN, m_comm);
    MPI_Allreduce(MPI_IN_PLACE, &max_step, 1, MPI_INT, MPI_MAX, m_comm);
    if (min_step != max_step) {
        throw std::runtime_error("Mismatch in module time steps.");
    }
    m_last_timestep = min_step;

    // Determine if any of the modules have adaptive timestepping
    for (auto& module : m_modules) {
        m_fixed_dt = m_fixed_dt && module->is_fixed_timestep_size();
    }
    MPI_Allreduce(MPI_IN_PLACE, &m_fixed_dt, 1, MPI_C_BOOL, MPI_LAND, m_comm);

    m_initialized = true;
}

void Simulation::perform_overset_connectivity()
{
    if (!m_use_overset) return;
    for (auto& module : m_modules)
        if (module->is_cfd()) module->call_pre_overset_conn_work();

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

    for (auto& module : m_modules)
        if (module->is_cfd()) module->call_post_overset_conn_work();
}

void Simulation::exchange_solution(bool increment_time)
{
    // SGF and UGF need their second pre-advance stage after connectivity.
    for (auto& module : m_modules)
        if (module->is_cfd()) module->call_prepare_solution_exchange();
    if (!m_use_overset) return;
    for (auto& module : m_modules)
        if (module->is_cfd()) module->call_register_solution();

    m_timers_tg.tick("SolExchange", increment_time);
    if (m_has_amr) {
        m_tg.dataUpdate_AMR();
    } else {
        const int row_major = 0;
        // assuming this pathway is kynema-ugf only and all instances have same
        // number of field components
        int ncomps = 0;
        for (auto& module : m_modules)
            if (module->is_cfd() && module->is_unstructured())
                ncomps = std::max(ncomps, module->get_ncomps());
        MPI_Allreduce(MPI_IN_PLACE, &ncomps, 1, MPI_INT, MPI_MAX, m_comm);
        m_tg.dataUpdate(ncomps, row_major);
    }
    m_timers_tg.tock("SolExchange");

    for (auto& module : m_modules)
        if (module->is_cfd()) module->call_update_solution();
}

void Simulation::run_timesteps(
    const int add_pic_its,
    const int nonlinear_its,
    const int nsteps,
    const double max_time)
{

    if (!m_initialized) {
        throw std::runtime_error("Simulation has not been initialized");
    }

    const int tstart = m_last_timestep + 1;
    const int tend = m_last_timestep + 1 + nsteps;
    m_printer.echo(
        "Running " + std::to_string(nsteps) + " timesteps starting from " +
        std::to_string(tstart));

    int nt = tstart;
    auto current_time = [&]() {
        double result = std::numeric_limits<double>::max();
        for (auto& module : m_modules) result = std::min(result, module->call_get_time());
        MPI_Allreduce(MPI_IN_PLACE, &result, 1, MPI_DOUBLE, MPI_MIN, m_comm);
        return result;
    };
    double time = max_time < 0. ? 0. : current_time();
    bool step_check = nsteps > 0 ? nt < tend : true;
    bool time_check = max_time > 0. ? time < max_time : true;
    bool do_step = step_check && time_check;
    while (do_step) {
        m_printer.echo_time_header();

        m_timers_kyn.tick("TimeStep");

        for (size_t inonlin = 0; inonlin < static_cast<size_t>(nonlinear_its);
             inonlin++) {

            bool increment_timer = inonlin > 0 ? true : false;

            for (auto& module : m_modules) {
                module->call_set_inputs(inonlin, increment_timer);
            }

            // Compute and communicate timestep (first iteration only, adaptive
            // dt)
            if (inonlin < 1 && !m_fixed_dt) {
                double dt{1e8};
                for (auto& module : m_modules) {
                    dt = std::min(dt, module->call_get_timestep_size());
                }
                MPI_Allreduce(
                    MPI_IN_PLACE, &dt, 1, MPI_DOUBLE, MPI_MIN, m_comm);
                for (auto& module : m_modules) module->call_set_timestep_size(dt);
            }

            for (auto& module : m_modules) {
                module->call_predict_inputs(inonlin, increment_timer);
            }

            // Overset connectivity update (external to modules)
            if (do_connectivity(nt)) perform_overset_connectivity();

            // Couple modules via solution exchange (external to modules)
            exchange_solution(increment_timer);

            if (inonlin > 0) {
                for (auto& module : m_modules)
                    module->call_reset_states(increment_timer);
            }

            for (auto& module : m_modules) {
                module->call_advance_states(inonlin, increment_timer);
            }

            for (auto& module : m_modules) {
                module->call_calculate_outputs(inonlin, increment_timer);
            }
        }

        if (add_pic_its > 0) {
            exchange_solution(true);
            for (auto& module : m_modules)
                module->call_additional_picard_iterations(add_pic_its);
        }
        for (auto& module : m_modules) module->call_finalize_time_step();

        MPI_Barrier(m_comm);

        m_timers_kyn.tock("TimeStep");

        MPI_Barrier(m_comm);

        print_timing(nt);

        MPI_Barrier(m_comm);

        mem_usage_all(nt);

        ++nt;
        if (max_time > 0.) time = current_time();
        step_check = nsteps > 0 ? nt < tend : true;
        time_check = max_time > 0. ? time < max_time : true;
        do_step = step_check && time_check;
    }
    for (auto& module : m_modules) module->call_dump_simulation_time();
    m_last_timestep = nt - 1;
}

bool Simulation::do_connectivity(const int tstep)
{
    return m_use_overset && (tstep > 0) &&
           (tstep % m_overset_update_interval) == 0;
}

void Simulation::print_timing(const int nt)
{
    // overall timestep timing
    auto timing_summary = m_timers_kyn.get_timings_summary(
        "Kynema", nt, m_comm, m_printer.io_rank());
    m_printer.echo(timing_summary);

    auto timing_detail = m_timers_kyn.get_timings_detail(
        "Kynema", nt, m_comm, m_printer.io_rank());
    m_printer.timing_to_file(timing_detail);

    MPI_Barrier(m_comm);

    // tioga timing
    timing_summary = m_timers_tg.get_timings_summary(
        "Tioga", nt, m_comm, m_printer.io_rank());
    m_printer.echo(timing_summary);

    timing_detail = m_timers_tg.get_timings_detail(
        "Tioga", nt, m_comm, m_printer.io_rank());
    m_printer.timing_to_file(timing_detail);

    // cfd module-specific timing
    for (auto& module : m_modules) {
        ParallelPrinter printer(module->comm());

        // summary timings
        std::string timings_summary = module->m_timers.get_timings_summary(
            module->identifier(), nt, module->comm(), printer.io_rank());
        printer.echo(timings_summary);

        // detailed timings
        std::string timings_detail = module->m_timers.get_timings_detail(
            module->identifier(), nt, module->comm(), printer.io_rank());
        printer.timing_to_file(timings_detail);
    }

    MPI_Barrier(m_comm);
}

long Simulation::mem_usage_all(const int step)
{
    const long mem = memory_usage();

    int psize;
    MPI_Comm_size(m_comm, &psize);

    // gather all memory usage
    std::vector<long> memall(psize);
    MPI_Gather(
        &mem, 1, MPI_LONG, memall.data(), 1, MPI_LONG, m_printer.io_rank(),
        m_comm);

    // FIXME: move to separate output files and put in Module
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
