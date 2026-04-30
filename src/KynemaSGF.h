#include "src/incflo.H"
#include "SGFTiogaIface.h"
#include "KynemaSolver.h"

namespace TIOGA {
class tioga;
}

namespace driver {

class KynemaSGF : public KynemaSolver
{
private:
    incflo m_incflo;
    SGFTiogaIface m_tgiface;
    std::vector<std::string> m_cell_vars;
    std::vector<std::string> m_node_vars;

public:
    static void
    initialize(MPI_Comm comm, const std::string& inpfile, std::ofstream& out);
    static void finalize();
    explicit KynemaSGF(
        const std::vector<std::string>&,
        const std::vector<std::string>&,
        TIOGA::tioga&);
    ~KynemaSGF();
    bool is_unstructured() override { return false; }
    bool is_amr() override { return true; }
    bool is_fixed_timestep_size() override;
    int overset_update_interval() override;
    int time_index() override;
    std::string identifier() override { return "Kynema-SGF"; }
    MPI_Comm comm() override { return m_comm; }

protected:
    void init_prolog(bool multi_solver_mode = true) override;
    void init_epilog() override;
    void prepare_solver_prolog() override;
    void prepare_solver_epilog() override;
    void pre_advance_stage0(size_t inonlin) override;
    void pre_advance_stage1(size_t inonlin) override;
    void pre_advance_stage2(size_t inonlin) override;
    double get_time() override;
    double get_timestep_size() override;
    void set_timestep_size(const double) override;
    void advance_timestep(size_t inonlin) override;
    void additional_picard_iterations(const int) override {};
    void post_advance() override;
    void pre_overset_conn_work() override;
    void post_overset_conn_work() override;
    void register_solution() override;
    void update_solution() override;
    void dump_simulation_time() override {};
    MPI_Comm m_comm;
};

} // namespace driver
