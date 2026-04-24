#include "KynemaSGF.h"
#include "amr-wind/incflo.H"
#include "amr-wind/CFDSim.H"
#include "amr-wind/core/SimTime.H"
#include "amr-wind/utilities/console_io.H"
#include "AMReX.H"
#include "AMReX_ParmParse.H"

#include "tioga.h"

namespace exawind {

void KynemaSGF::initialize(
    MPI_Comm comm, const std::string& inpfile, std::ofstream& out)
{
    int argc = 2;
    char** argv = new char*[argc];

    const char* exename = "kynema_sgf";
    argv[0] = const_cast<char*>(exename);
    argv[1] = const_cast<char*>(inpfile.c_str());

    amrex::Initialize(
        argc, argv, true, comm,
        []() {
            amrex::ParmParse pp("amrex");
            // Set the defaults so that we throw an exception instead of
            // attempting to generate backtrace files. However, if the user has
            // explicitly set these options in their input files respect those
            // settings.
            if (!pp.contains("throw_exception")) pp.add("throw_exception", 1);
            if (!pp.contains("signal_handling")) pp.add("signal_handling", 0);
        },
        out, out);

    kynema_sgf::io::print_banner(comm, amrex::OutStream());

    delete[] argv;
}

void KynemaSGF::finalize() { amrex::Finalize(); }

KynemaSGF::KynemaSGF(
    const std::vector<std::string>& cell_vars,
    const std::vector<std::string>& node_vars,
    TIOGA::tioga& tg)
    : m_incflo()
    , m_tgiface(m_incflo.sim(), tg)
    , m_cell_vars(cell_vars)
    , m_node_vars(node_vars)
    , m_comm(amrex::ParallelContext::CommunicatorSub())
{
    m_incflo.sim().activate_overset();
}

KynemaSGF::~KynemaSGF() = default;

void KynemaSGF::init_prolog(bool)
{
    m_incflo.init_mesh();
    m_incflo.init_kynema_sgf_modules();
}

void KynemaSGF::init_epilog() {}

void KynemaSGF::prepare_solver_prolog() {}

void KynemaSGF::prepare_solver_epilog()
{
    m_incflo.prepare_for_time_integration();
}

void KynemaSGF::pre_advance_stage0(size_t inonlin)
{
    if (inonlin < 1) {
        m_incflo.sim().time().new_timestep();
        m_incflo.regrid_and_update();
        m_incflo.compute_dt();
    }
}

void KynemaSGF::pre_advance_stage1(size_t inonlin)
{
    if (inonlin < 1) {
        m_incflo.pre_advance_stage1();
    }
}

void KynemaSGF::pre_advance_stage2(size_t inonlin)
{
    if (inonlin < 1) m_incflo.pre_advance_stage2();
}

double KynemaSGF::get_time() { return m_incflo.time().new_time(); }

double KynemaSGF::get_timestep_size() { return m_incflo.time().delta_t(); }

void KynemaSGF::set_timestep_size(const double dt)
{
    m_incflo.sim().time().delta_t() = dt;
}

bool KynemaSGF::is_fixed_timestep_size()
{
    return (!m_incflo.sim().time().adaptive_timestep());
}

void KynemaSGF::advance_timestep(size_t inonlin) { m_incflo.do_advance(inonlin); }

void KynemaSGF::post_advance() { m_incflo.post_advance_work(); }

void KynemaSGF::pre_overset_conn_work() { m_tgiface.pre_overset_conn_work(); }

void KynemaSGF::post_overset_conn_work() { m_tgiface.post_overset_conn_work(); }

void KynemaSGF::register_solution()
{
    m_tgiface.register_solution(m_cell_vars, m_node_vars);
}

void KynemaSGF::update_solution() { m_tgiface.update_solution(); }

int KynemaSGF::overset_update_interval()
{
    const int regrid_int = m_incflo.sim().time().regrid_interval();
    return regrid_int > 0 ? regrid_int : 100000000;
}

int KynemaSGF::time_index() { return m_incflo.sim().time().time_index(); }

} // namespace exawind
