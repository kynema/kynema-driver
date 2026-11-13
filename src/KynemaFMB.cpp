#include "KynemaFMB.h"
#include "mpi.h"
#include <iostream>
#include <stdexcept>

// Include FMB interface
#include "interfaces/generalized/generalized_structure_interface.hpp"

namespace driver {

KynemaFMB::KynemaFMB(const YAML::Node& fmb_config, MPI_Comm comm, int id)
    : m_comm(comm)
    , m_id(id)
    , m_fmb_config(fmb_config)
{
    MPI_Comm_rank(m_comm, &m_prank);
    m_dt = fmb_config["simulation"]["step_size"].as<double>();
}

KynemaFMB::~KynemaFMB() = default;

void KynemaFMB::init_prolog(bool)
{
    // Create FMB interface if not already created
    if (!m_fmb_interface) {
        // FMB interface constructor requires just the YAML config
        // It will initialize the structural model
        m_fmb_interface =
            std::make_unique<kynema_fmb::interfaces::GeneralStructureInterface>(
                m_fmb_config);
    }
}

void KynemaFMB::init_epilog()
{
    // Call FMB's initialize_module to finalize initialization
    if (m_fmb_interface && !m_initialized) {
        auto module_description = m_fmb_interface->initialize_module();
        m_num_nodes = module_description["model"]["num_nodes"].as<size_t>();
        m_forces.resize(m_num_nodes);
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

void KynemaFMB::set_forces(const std::vector<std::array<double, 6>>& forces)
{
    if (!m_initialized || forces.size() != m_num_nodes) {
        throw std::invalid_argument(
            "FMB forces must match the initialized node count");
    }
    m_forces = forces;
}

std::array<double, 7> KynemaFMB::node_position(size_t node) const
{
    if (!m_initialized || node >= m_num_nodes) {
        throw std::out_of_range("Invalid FMB node");
    }
    const auto& x = m_fmb_interface->get_outputs().x;
    std::array<double, 7> result;
    for (size_t i = 0; i < result.size(); ++i) result[i] = x(node, i);
    return result;
}

void KynemaFMB::set_inputs(size_t inonlin)
{
    // The first iteration uses predicted forces; subsequent iterations use
    // the latest coupled forces.
    if (inonlin == 0) return;
    using Interface = kynema_fmb::interfaces::GeneralStructureInterface;
    Interface::HostView<double* [6]> forces("forces", m_num_nodes);
    for (size_t node = 0; node < m_num_nodes; ++node) {
        for (size_t component = 0; component < 6; ++component) {
            forces(node, component) = m_forces[node][component];
        }
    }
    m_fmb_interface->receive_inputs(forces);
}

void KynemaFMB::predict_inputs(size_t inonlin)
{
    if (inonlin == 0) m_fmb_interface->predict_inputs();
}

void KynemaFMB::advance_states(size_t inonlin)
{
    if (inonlin > 0) m_fmb_interface->reset_states();
    m_fmb_interface->advance_states();
}

void KynemaFMB::calculate_outputs(size_t)
{
    m_fmb_interface->calculate_outputs();
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
    if (dt != m_dt) {
        throw std::runtime_error(
            "KynemaFMB requires its configured fixed timestep");
    }
}

void KynemaFMB::additional_picard_iterations(const int n)
{
    // For additional Picard iterations, we need to:
    // 1. Reset to previous state
    // 2. Advance again (this is what the test case shows)
    for (int i = 0; i < n; ++i) {
        m_fmb_interface->reset_states();
        m_fmb_interface->advance_states();
    }
    m_fmb_interface->calculate_outputs();
}

void KynemaFMB::finalize_time_step()
{
    m_fmb_interface->finalize_time_step();
    m_time += m_dt;
    ++m_time_step;
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
