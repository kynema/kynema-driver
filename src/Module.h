#ifndef DRIVER_MODULE_H
#define DRIVER_MODULE_H

#include "Timers.h"
#include "ParallelPrinter.h"

namespace driver {

class Module
{
public:
    explicit Module() : m_timers(m_names) {};
    virtual ~Module() = default;

    void call_init_prolog(bool multi_module_mode = true)
    {
        init_prolog(multi_module_mode);
    };
    void call_init_epilog() { init_epilog(); };
    void call_prepare_module_prolog() { prepare_module_prolog(); };
    void call_prepare_module_epilog() { prepare_module_epilog(); };
    double call_get_time()
    {
        const std::string name = "Pre";
        m_timers.tick(name);
        double time = get_time();
        m_timers.tock(name);
        return time;
    }
    double call_get_timestep_size()
    {
        const std::string name = "Pre";
        m_timers.tick(name);
        double dt = get_timestep_size();
        m_timers.tock(name);
        return dt;
    };
    void call_set_timestep_size(double dt)
    {
        const std::string name = "Pre";
        m_timers.tick(name);
        set_timestep_size(dt);
        m_timers.tock(name);
    };
    void call_additional_picard_iterations(const int n)
    {
        std::string name = "AdditionalPicardIterations";
        if (std::find(m_names.begin(), m_names.end(), name) == m_names.end()) {
            m_timers.addTimer(name);
            m_names.push_back(name);
        }
        m_timers.tick(name);
        additional_picard_iterations(n);
        m_timers.tock(name);
    };
    void call_finalize_time_step()
    {
        const std::string name = "Post";
        m_timers.tick(name);
        finalize_time_step();
        m_timers.tock(name);
    };
    void call_pre_overset_conn_work()
    {
        const std::string name = "PreConn";
        m_timers.tick(name);
        pre_overset_conn_work();
        m_timers.tock(name);
    };
    void call_post_overset_conn_work()
    {
        const std::string name = "PostConn";
        m_timers.tick(name);
        post_overset_conn_work();
        m_timers.tock(name);
    };
    void call_register_solution()
    {
        const std::string name = "Register";
        m_timers.tick(name);
        register_solution();
        m_timers.tock(name);
    };
    void call_prepare_solution_exchange() { prepare_solution_exchange(); }
    void call_update_solution()
    {
        const std::string name = "Update";
        m_timers.tick(name);
        update_solution();
        m_timers.tock(name);
    };

    void call_dump_simulation_time() { dump_simulation_time(); };

    void call_set_inputs(size_t inonlin, const bool increment)
    {
        const std::string name = "SetInputs";
        m_timers.tick(name, increment);
        set_inputs(inonlin);
        m_timers.tock(name);
    }
    void call_predict_inputs(size_t inonlin, const bool increment)
    {
        const std::string name = "PredictInputs";
        m_timers.tick(name, increment);
        predict_inputs(inonlin);
        m_timers.tock(name);
    }
    void call_advance_states(size_t inonlin, const bool increment)
    {
        const std::string name = "AdvanceStates";
        m_timers.tick(name, increment);
        advance_states(inonlin);
        m_timers.tock(name);
    }
    void call_reset_states(const bool increment)
    {
        const std::string name = "ResetStates";
        m_timers.tick(name, increment);
        reset_states();
        m_timers.tock(name);
    }
    void call_calculate_outputs(size_t inonlin, const bool increment)
    {
        const std::string name = "CalculateOutputs";
        m_timers.tick(name, increment);
        calculate_outputs(inonlin);
        m_timers.tock(name);
    }

    virtual bool is_cfd() { return false; };
    virtual bool is_unstructured() { return false; };
    virtual bool is_amr() { return false; };
    virtual bool is_fixed_timestep_size() = 0;
    virtual int overset_update_interval() { return 100000000; };
    virtual int time_index() = 0;
    virtual std::string identifier() { return "Module"; }
    virtual MPI_Comm comm() = 0;
    virtual int get_ncomps() { return 0; };
    void timing_details();
    //! Timer names
    std::vector<std::string> m_names{
        "Pre",         "PreConn",       "PostConn",        "Register",
        "Update",      "Post",          "SetInputs",       "PredictInputs",
        "ResetStates", "AdvanceStates", "CalculateOutputs"};
    //! Timers
    Timers m_timers;

protected:
    virtual void init_prolog(bool multi_module_mode = true) = 0;
    virtual void init_epilog() = 0;
    virtual void prepare_module_prolog() = 0;
    virtual void prepare_module_epilog() = 0;
    virtual double get_time() = 0;
    virtual double get_timestep_size() = 0;
    virtual void set_timestep_size(const double) = 0;
    virtual void additional_picard_iterations(const int) = 0;
    virtual void finalize_time_step() = 0;
    virtual void pre_overset_conn_work() {}
    virtual void post_overset_conn_work() {}
    virtual void register_solution() {}
    virtual void prepare_solution_exchange() {}
    virtual void update_solution() {}
    virtual void dump_simulation_time() = 0;

    virtual void set_inputs(size_t inonlin) = 0;
    virtual void predict_inputs(size_t inonlin) = 0;
    virtual void reset_states() = 0;
    virtual void advance_states(size_t inonlin) = 0;
    virtual void calculate_outputs(size_t inonlin) = 0;
};

} // namespace driver
#endif /* DRIVER_MODULE_H */
