#ifndef KYNEMAUGF_H
#define KYNEMAUGF_H

#include <vector>
#include <string>

#include "Simulation.h"
#include "stk_util/parallel/Parallel.hpp"
#include "yaml-cpp/yaml.h"
#include "KynemaSolver.h"

namespace TIOGA {
class tioga;
}

namespace driver {

class KynemaUGF : public KynemaSolver
{
private:
    YAML::Node m_doc;
    sierra::kynema_ugf::Simulation m_sim;
    std::vector<std::string> m_fnames;
    int m_ncomps;
    int m_id;

public:
    static void initialize();
    static void finalize();
    static std::string change_file_name_suffix(
        std::string inpfile, std::string suffix, int index = -1)
    {
        int extloc = inpfile.rfind(".");
        std::string logfile = inpfile;
        if (index >= 0) {
            suffix = "_" + std::to_string(index) + suffix;
        }
        if (extloc != static_cast<int>(std::string::npos)) {
            logfile = inpfile.substr(0, extloc) + suffix;
        }
        return logfile;
    }
    explicit KynemaUGF(
        int id,
        stk::ParallelMachine comm,
        const YAML::Node& inp_yaml,
        const std::string& logfile,
        const std::vector<std::string>& fnames,
        TIOGA::tioga& tg);
    ~KynemaUGF();
    bool is_unstructured() override { return true; }
    bool is_amr() override { return false; }
    bool is_fixed_timestep_size() override;
    int overset_update_interval() override;
    int time_index() override;
    std::string identifier() override
    {
        return ("Kynema-UGF-" + std::to_string(m_id));
    }
    MPI_Comm comm() override { return m_comm; }
    int get_ncomps() override { return m_ncomps; }

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
    void additional_picard_iterations(const int) override;
    void post_advance() override;
    void pre_overset_conn_work() override;
    void post_overset_conn_work() override;
    void register_solution() override;
    void update_solution() override;
    void dump_simulation_time() override;
    MPI_Comm m_comm;
};

} // namespace driver

#endif /* KYNEMAUGF_H */
