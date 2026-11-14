#include "src/incflo.H"
#include "SGFTiogaIface.h"
#include "../Module.h"

namespace TIOGA {
class tioga;
}

namespace driver {

class KynemaSGF : public Module
{
private:
    incflo m_incflo;
    SGFTiogaIface m_tgiface;
    std::vector<std::string> m_cell_vars;
    std::vector<std::string> m_node_vars;
    bool m_stage2_pending{false};

public:
    static void
    initialize(MPI_Comm comm, const std::string& inpfile, std::ofstream& out);
    static void finalize();
    explicit KynemaSGF(
        const std::vector<std::string>&,
        const std::vector<std::string>&,
        TIOGA::tioga&);
    ~KynemaSGF();
    bool is_cfd() override { return true; }
    bool is_unstructured() override { return false; }
    bool is_amr() override { return true; }
    bool is_fixed_timestep_size() override;
    int overset_update_interval() override;
    int time_index() override;
    std::string identifier() override { return "Kynema-SGF"; }
    MPI_Comm comm() override { return m_comm; }

protected:
    void init_prolog(bool multi_module_mode = true) override;
    void init_epilog() override;
    void prepare_module_prolog() override;
    void prepare_module_epilog() override;
    void set_inputs(size_t inonlin) override;
    void predict_inputs(size_t inonlin) override;
    void reset_states() override {}
    void advance_states(size_t inonlin) override;
    void calculate_outputs(size_t) override {}
    double get_time() override;
    double get_timestep_size() override;
    void set_timestep_size(const double) override;
    void additional_picard_iterations(const int) override {};
    void finalize_time_step() override;
    void pre_overset_conn_work() override;
    void post_overset_conn_work() override;
    void prepare_solution_exchange() override;
    void register_solution() override;
    void update_solution() override;
    void dump_simulation_time() override {};
    MPI_Comm m_comm;
};

} // namespace driver
