#ifndef KYNEMASOLVER_H
#define KYNEMASOLVER_H

#include "Timers.h"
#include "ParallelPrinter.h"

namespace driver {

class KynemaSolver
{
public:
    explicit KynemaSolver() : m_timers(m_names) {};
    virtual ~KynemaSolver();

    void call_init_prolog(bool multi_solver_mode = true)
    {
        init_prolog(multi_solver_mode);
    };
    void call_init_epilog() { init_epilog(); };
    void call_prepare_solver_prolog() { prepare_solver_prolog(); };
    void call_prepare_solver_epilog() { prepare_solver_epilog(); };
    void call_pre_advance_stage0(size_t inonlin, const bool increment)
    {
        const std::string name = "Pre";
        m_timers.tick(name, increment);
        pre_advance_stage0(inonlin);
        m_timers.tock(name);
    }
    void call_pre_advance_stage1(size_t inonlin, const bool increment)
    {
        const std::string name = "Pre";
        m_timers.tick(name, increment);
        pre_advance_stage1(inonlin);
        m_timers.tock(name);
    };
    void call_pre_advance_stage2(size_t inonlin, const bool increment)
    {
        const std::string name = "Pre";
        m_timers.tick(name, increment);
        pre_advance_stage2(inonlin);
        m_timers.tock(name);
    };
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
    void call_advance_timestep(size_t inonlin, const bool increment)
    {
        const std::string name = "Solve";
        m_timers.tick(name, increment);
        advance_timestep(inonlin);
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
    void call_post_advance()
    {
        const std::string name = "Post";
        m_timers.tick(name);
        post_advance();
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
    void call_update_solution()
    {
        const std::string name = "Update";
        m_timers.tick(name);
        update_solution();
        m_timers.tock(name);
    };

    void call_dump_simulation_time() { dump_simulation_time(); };

    virtual bool is_unstructured() { return false; };
    virtual bool is_amr() { return false; };
    virtual bool is_fixed_timestep_size() = 0;
    virtual int overset_update_interval() { return 100000000; };
    virtual int time_index() = 0;
    virtual std::string identifier() { return "KynemaSolver"; }
    virtual MPI_Comm comm() = 0;
    virtual int get_ncomps() { return 0; };
    void timing_details();
    //! Timer names
    std::vector<std::string> m_names{
        "Pre", "PreConn", "PostConn", "Register", "Update", "Solve", "Post"};
    //! Timers
    Timers m_timers;

protected:
    virtual void init_prolog(bool multi_solver_mode = true) = 0;
    virtual void init_epilog() = 0;
    virtual void prepare_solver_prolog() = 0;
    virtual void prepare_solver_epilog() = 0;
    virtual void pre_advance_stage0(size_t inonlin) = 0;
    virtual void pre_advance_stage1(size_t inonlin) = 0;
    virtual void pre_advance_stage2(size_t inonlin) = 0;
    virtual double get_time() = 0;
    virtual double get_timestep_size() = 0;
    virtual void set_timestep_size(const double) = 0;
    virtual void advance_timestep(size_t inonlin) = 0;
    virtual void additional_picard_iterations(const int) = 0;
    virtual void post_advance() = 0;
    virtual void pre_overset_conn_work() = 0;
    virtual void post_overset_conn_work() = 0;
    virtual void register_solution() = 0;
    virtual void update_solution() = 0;
    virtual void dump_simulation_time() = 0;
};

} // namespace driver
#endif /* KYNEMASOLVER_H */
