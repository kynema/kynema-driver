#include "KynemaFMB.h"
#include "mpi.h"
#include <iostream>

// Include FMB interface
#include "interfaces/generalized/generalized_structure_interface.hpp"

namespace driver {

KynemaFMB::KynemaFMB(const YAML::Node& fmb_config, MPI_Comm comm)
    : m_comm(comm)
    , m_fmb_config(fmb_config)
{
    MPI_Comm_rank(m_comm, &m_prank);
    MPI_Comm_size(m_comm, &m_psize);

    // Extract timestep if specified
    if (fmb_config["time_step"]) {
        m_dt = fmb_config["time_step"].as<double>();
    }
    if (fmb_config["start_time"]) {
        m_time = fmb_config["start_time"].as<double>();
    }
}

KynemaFMB::~KynemaFMB() = default;

void KynemaFMB::init_prolog(bool multi_solver_mode)
{
    // Create FMB interface if not already created
    if (!m_fmb_interface) {
        // FMB interface constructor requires just the YAML config
        // It will initialize the structural model
        m_fmb_interface = std::make_unique<kynema_fmb::interfaces::GeneralStructureInterface>(
            m_fmb_config);
    }
}

void KynemaFMB::init_epilog()
{
    // Call FMB's initialize_module to finalize initialization
    if (m_fmb_interface && !m_initialized) {
        auto module_description = m_fmb_interface->initialize_module();
        m_initialized = true;

        if (m_prank == 0) {
            std::cout << "KynemaFMB initialized successfully\n";
        }
    }
}

void KynemaFMB::prepare_solver_prolog()
{
    // No special preparation needed for FMB
}

void KynemaFMB::prepare_solver_epilog()
{
    // No special preparation needed for FMB
}

void KynemaFMB::set_inputs(size_t inonlin)
{
    // On first nonlinear iteration, receive updated forces from coupling
    if (inonlin == 0 && m_fmb_interface) {
        // Forces are set via receive_inputs()
        // This is called after external coupling provides force data
    }
}

void KynemaFMB::predict_inputs(size_t inonlin)
{
    if (!m_fmb_interface) return;

    // Predict the inputs at the next time step
    m_fmb_interface->predict_inputs();
}

void KynemaFMB::advance_states(size_t inonlin)
{
    if (!m_fmb_interface) return;

    // Solve the structural dynamics equations
    m_fmb_interface->advance_states();

    // Advance time tracking
    m_time += m_dt;
    m_time_step++;
}

void KynemaFMB::calculate_outputs(size_t inonlin)
{
    if (!m_fmb_interface) return;

    // Calculate outputs (forces, stresses, etc.)
    m_fmb_interface->calculate_outputs();
}

// ===== Legacy interface methods (for compatibility) =====

void KynemaFMB::pre_advance_stage0(size_t inonlin)
{
    // Default: delegates to set_inputs via virtual methods
    set_inputs(inonlin);
}

void KynemaFMB::pre_advance_stage1(size_t inonlin)
{
    // Default: delegates to predict_inputs via virtual methods
    predict_inputs(inonlin);
}

void KynemaFMB::pre_advance_stage2(size_t inonlin)
{
    // For FMB, stage2 is empty (prediction already done in stage1)
}

double KynemaFMB::get_time()
{
    return m_time;
}

double KynemaFMB::get_timestep_size()
{
    return m_dt;
}

void KynemaFMB::set_timestep_size(const double dt)
{
    m_dt = dt;
}

void KynemaFMB::advance_timestep(size_t inonlin)
{
    // This is the old interface; new code uses advance_states()
    advance_states(inonlin);
}

void KynemaFMB::additional_picard_iterations(const int n)
{
    if (!m_fmb_interface) return;

    // For additional Picard iterations, we need to:
    // 1. Reset to previous state
    // 2. Advance again (this is what the test case shows)
    for (int i = 0; i < n; ++i) {
        m_fmb_interface->reset_states();
        m_fmb_interface->advance_states();
    }
}

void KynemaFMB::post_advance()
{
    // This is called after timestep; for FMB we finalize
    if (m_fmb_interface) {
        m_fmb_interface->finalize_time_step();
    }
}

void KynemaFMB::pre_overset_conn_work()
{
    // FMB doesn't participate in overset operations
}

void KynemaFMB::post_overset_conn_work()
{
    // FMB doesn't participate in overset operations
}

void KynemaFMB::register_solution()
{
    // No solution registration needed for FMB
}

void KynemaFMB::update_solution()
{
    // Solution already updated in advance_states
}

void KynemaFMB::dump_simulation_time()
{
    if (m_prank == 0) {
        std::cout << "KynemaFMB final time: " << m_time << " seconds\n";
    }
}

bool KynemaFMB::is_fixed_timestep_size()
{
    return true; // FMB uses fixed timesteps
}

int KynemaFMB::time_index()
{
    return m_time_step;
}

} // namespace driver
