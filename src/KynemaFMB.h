#ifndef KYNEMAFMB_H
#define KYNEMAFMB_H

#include "KynemaSolver.h"
#include "mpi.h"
#include "yaml-cpp/yaml.h"
#include <memory>

// Forward declaration to avoid circular dependencies
namespace kynema_fmb::interfaces {
class GeneralStructureInterface;
}

namespace driver {

/**
 * @brief Kynema-FMB structural dynamics solver wrapper
 *
 * This class wraps the GeneralStructureInterface from Kynema-FMB
 * to conform to the modular solver interface.
 */
class KynemaFMB : public KynemaSolver
{
public:
    /**
     * @brief Construct FMB solver with YAML configuration
     * @param fmb_config YAML node containing FMB configuration
     * @param comm MPI communicator for the solver
     */
    explicit KynemaFMB(const YAML::Node& fmb_config, MPI_Comm comm);

    ~KynemaFMB() override;

    // Initialization methods (from base class)
    void init_prolog(bool multi_solver_mode = true) override;
    void init_epilog() override;
    void prepare_solver_prolog() override;
    void prepare_solver_epilog() override;

    // Modular interface methods (new pattern)
    void set_inputs(size_t inonlin) override;
    void predict_inputs(size_t inonlin) override;
    void advance_states(size_t inonlin) override;
    void calculate_outputs(size_t inonlin) override;

    // Legacy interface methods (for compatibility, may not all be used)
    void pre_advance_stage0(size_t inonlin) override;
    void pre_advance_stage1(size_t inonlin) override;
    void pre_advance_stage2(size_t inonlin) override;
    double get_time() override;
    double get_timestep_size() override;
    void set_timestep_size(const double dt) override;
    void advance_timestep(size_t inonlin) override;
    void additional_picard_iterations(const int n) override;
    void post_advance() override;
    void pre_overset_conn_work() override;
    void post_overset_conn_work() override;
    void register_solution() override;
    void update_solution() override;
    void dump_simulation_time() override;

    // Query methods
    bool is_fixed_timestep_size() override;
    int time_index() override;
    std::string identifier() override { return "KynemaFMB"; }
    MPI_Comm comm() override { return m_comm; }

private:
    MPI_Comm m_comm;
    int m_prank{0};
    int m_psize{1};

    // FMB state
    std::unique_ptr<kynema_fmb::interfaces::GeneralStructureInterface> m_fmb_interface;
    YAML::Node m_fmb_config;
    double m_time{0.0};
    double m_dt{0.01};
    int m_time_step{0};
    bool m_initialized{false};

    // Workspace for force coupling
    std::vector<std::vector<double>> m_forces_buffer;
};

} // namespace driver
#endif /* KYNEMAFMB_H */
