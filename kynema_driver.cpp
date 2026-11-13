#include "src/Simulation.h"
#include "src/MPIUtilities.h"
#include "src/yaml-editor.h"
#include <yaml-cpp/yaml.h>

#ifdef KYNEMA_DRIVER_ENABLE_SGF
#include "src/modules/KynemaSGF.h"
#endif
#ifdef KYNEMA_DRIVER_ENABLE_UGF
#include "src/modules/KynemaUGF.h"
#endif
#ifdef KYNEMA_DRIVER_ENABLE_FMB
#include "src/modules/KynemaFMB.h"
#include <Kokkos_Core.hpp>
#endif
#ifdef KYNEMA_DRIVER_ENABLE_ROCM
#include <hip/hip_runtime.h>
#endif

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string usage(const char* name)
{
    return std::string("usage: ") + name +
           " [--sgf NPROCS] [--ugf NPROCS] [--fmb NPROCS] input_file\n"
           "  Each selected module type defaults to all MPI ranks.\n"
           "  Instances of a module type partition its ranks; module types may "
           "overlap.\n"
           "  YAML keys: kynema_sgf_inp, kynema_ugf_inp, kynema_fmb_inp.\n"
           "  UGF and FMB accept a filename or list; ugf_procs and fmb_procs\n"
           "  optionally specify the rank count per instance.\n"
           "  Modules are enabled at build time with "
           "KYNEMA_DRIVER_ENABLE_SGF,\n"
           "  KYNEMA_DRIVER_ENABLE_UGF and KYNEMA_DRIVER_ENABLE_FMB.\n";
}

struct Options
{
    int sgf, ugf, fmb;
    bool sgf_set{false}, ugf_set{false}, fmb_set{false};
    bool help{false};
    std::string input;
};

Options parse_options(int argc, char** argv, int size)
{
    Options result{size, size, size, false, false, false, false, {}};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            result.help = true;
        } else if (arg == "--sgf" || arg == "--ugf" || arg == "--fmb") {
            if (++i == argc)
                throw std::runtime_error(arg + " requires a rank count");
            const std::string value = argv[i];
            int count = 0;
            const auto parsed = std::from_chars(
                value.data(), value.data() + value.size(), count);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != value.data() + value.size() || count < 1 ||
                count > size)
                throw std::runtime_error(
                    arg + " rank count must be between 1 and MPI size");
            int* target = arg == "--sgf"   ? &result.sgf
                          : arg == "--ugf" ? &result.ugf
                                           : &result.fmb;
            bool* specified = arg == "--sgf"   ? &result.sgf_set
                              : arg == "--ugf" ? &result.ugf_set
                                               : &result.fmb_set;
            if (*specified)
                throw std::runtime_error(arg + " specified more than once");
            *target = count;
            *specified = true;
        } else if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("Unknown option: " + arg);
        } else if (result.input.empty()) {
            result.input = arg;
        } else {
            throw std::runtime_error(
                "Only one driver input file may be supplied");
        }
    }
    if (!result.help && result.input.empty())
        throw std::runtime_error(usage(argv[0]));
    return result;
}

YAML::Node input_list(const YAML::Node& node, const char* key)
{
    YAML::Node result(YAML::NodeType::Sequence);
    const YAML::Node input = node[key];
    if (!input) return result;
    if (input.IsScalar())
        result.push_back(input);
    else if (input.IsSequence())
        result = input;
    else
        throw std::runtime_error(
            std::string(key) + " must be a filename or sequence");
    if (result.size() == 0)
        throw std::runtime_error(std::string(key) + " cannot be empty");
    return result;
}

std::vector<int>
partition(const YAML::Node& node, const char* key, int count, int ranks)
{
    if (count == 0) return {};
    if (ranks < count)
        throw std::runtime_error(
            std::string(key) + ": not enough ranks for instances");
    std::vector<int> sizes;
    if (node[key]) {
        sizes = node[key].as<std::vector<int>>();
        if (static_cast<int>(sizes.size()) != count ||
            std::any_of(
                sizes.begin(), sizes.end(), [](int n) { return n < 1; }) ||
            std::accumulate(sizes.begin(), sizes.end(), 0) != ranks)
            throw std::runtime_error(
                std::string(key) +
                " must give one positive rank count per instance summing to "
                "the CLI count");
    } else {
        sizes.assign(count, ranks / count);
        for (int i = count - ranks % count; i < count; ++i) ++sizes[i];
    }
    return sizes;
}

std::vector<MPI_Comm> communicators(const std::vector<int>& sizes, int size)
{
    std::vector<MPI_Comm> result;
    int start = size - std::accumulate(sizes.begin(), sizes.end(), 0);
    for (int n : sizes) {
        result.push_back(driver::create_subcomm(MPI_COMM_WORLD, n, start));
        start += n;
    }
    return result;
}

bool participating(const std::vector<MPI_Comm>& comms)
{
    return std::any_of(comms.begin(), comms.end(), [](MPI_Comm c) {
        return c != MPI_COMM_NULL;
    });
}

void free_comms(std::vector<MPI_Comm>& comms)
{
    for (auto& comm : comms)
        if (comm != MPI_COMM_NULL) MPI_Comm_free(&comm);
}

int run(int argc, char** argv, int size, int rank)
{
    const auto options = parse_options(argc, argv, size);
    if (options.help) {
        if (rank == 0) std::cout << usage(argv[0]);
        return 0;
    }
    const YAML::Node doc = YAML::LoadFile(options.input);
    const YAML::Node node = doc["kynema_driver"];
    if (!node || !node.IsMap())
        throw std::runtime_error("Missing kynema_driver mapping");
    const bool sgf = static_cast<bool>(node["kynema_sgf_inp"]);
    const YAML::Node ugf = input_list(node, "kynema_ugf_inp");
    const YAML::Node fmb = input_list(node, "kynema_fmb_inp");
    if (!sgf && ugf.size() == 0 && fmb.size() == 0)
        throw std::runtime_error("No modules specified in kynema_driver");
    if (static_cast<size_t>(sgf) + ugf.size() > 1) {
        if (ugf.size() && !node["ugf_vars"])
            throw std::runtime_error("Overset UGF requires ugf_vars");
        if (sgf && (!node["sgf_cell_vars"] || !node["sgf_node_vars"]))
            throw std::runtime_error(
                "Overset SGF requires sgf_cell_vars and sgf_node_vars");
    }
    if ((options.sgf_set && !sgf) || (options.ugf_set && ugf.size() == 0) ||
        (options.fmb_set && fmb.size() == 0))
        throw std::runtime_error(
            "Rank option provided for a module not selected in YAML");
#ifndef KYNEMA_DRIVER_ENABLE_SGF
    if (sgf)
        throw std::runtime_error("Kynema-SGF is not enabled in this build");
#endif
#ifndef KYNEMA_DRIVER_ENABLE_UGF
    if (ugf.size())
        throw std::runtime_error("Kynema-UGF is not enabled in this build");
#endif
#ifndef KYNEMA_DRIVER_ENABLE_FMB
    if (fmb.size())
        throw std::runtime_error("Kynema-FMB is not enabled in this build");
#endif
    const int steps =
        node["num_timesteps"] ? node["num_timesteps"].as<int>() : -1;
    const double max_time =
        node["max_time"] ? node["max_time"].as<double>() : -1.;
    const int picard = node["additional_picard_iterations"]
                           ? node["additional_picard_iterations"].as<int>()
                           : 0;
    const int nonlinear = node["nonlinear_iterations"]
                              ? node["nonlinear_iterations"].as<int>()
                              : 1;
    if ((steps <= 0 && max_time <= 0.) || nonlinear < 1 || picard < 0)
        throw std::runtime_error(
            "Specify positive num_timesteps or max_time, positive "
            "nonlinear_iterations and nonnegative "
            "additional_picard_iterations");

    auto ugf_comms = communicators(
        partition(node, "ugf_procs", ugf.size(), options.ugf), size);
    auto fmb_comms = communicators(
        partition(node, "fmb_procs", fmb.size(), options.fmb), size);
    MPI_Comm sgf_comm =
        sgf ? driver::create_subcomm(MPI_COMM_WORLD, options.sgf, 0)
            : MPI_COMM_NULL;
    const bool has_ugf = participating(ugf_comms);
    const bool has_fmb = participating(fmb_comms);
    (void)has_ugf;
    (void)has_fmb;
    std::ofstream out;

    {
        driver::Simulation sim(MPI_COMM_WORLD);
        sim.set_holemap_alg(
            node["use_adaptive_holemap"]
                ? node["use_adaptive_holemap"].as<bool>()
                : false);
        if (node["composite_body"]) {
            const auto body = node["composite_body"];
            sim.set_composite_num(body.size());
            for (size_t i = 0; i < body.size(); ++i) {
                const auto item = body[i];
                sim.set_composite_body(
                    i, item["num_body_tags"].as<int>(),
                    item["body_tags"].as<std::vector<int>>(),
                    item["dominance_tags"].as<std::vector<int>>(),
                    item["search_tolerance"].as<double>());
            }
        }
#ifdef KYNEMA_DRIVER_ENABLE_SGF
        if (sgf_comm != MPI_COMM_NULL) {
            const auto file = node["kynema_sgf_inp"].as<std::string>();
            auto dot = file.find_last_of('.');
            out.open(file.substr(0, dot) + ".log");
            if (!out) throw std::runtime_error("Cannot open SGF log");
            driver::KynemaSGF::initialize(sgf_comm, file, out);
        }
#endif
#ifdef KYNEMA_DRIVER_ENABLE_UGF
        if (has_ugf) driver::KynemaUGF::initialize();
#endif
#ifdef KYNEMA_DRIVER_ENABLE_FMB
        if (has_fmb && !Kokkos::is_initialized()) Kokkos::initialize();
#endif
#ifdef KYNEMA_DRIVER_ENABLE_UGF
        for (size_t i = 0; i < ugf.size(); ++i) {
            if (ugf_comms[i] == MPI_COMM_NULL) continue;
            const auto instance = ugf[i];
            const bool mapped = instance.IsMap();
            const auto file =
                mapped ? instance["base_input_file"].as<std::string>()
                       : instance.as<std::string>();
            const auto logfile =
                mapped && instance["logfile"]
                    ? instance["logfile"].as<std::string>()
                    : driver::KynemaUGF::change_file_name_suffix(
                          file, ".log", mapped ? static_cast<int>(i) : -1);
            YAML::Node config = YAML::LoadFile(file);
            if (node["ugf_replace_all"])
                YEDIT::find_and_replace(config, node["ugf_replace_all"]);
            if (mapped && instance["replace"])
                YEDIT::find_and_replace(config, instance["replace"]);
            int comm_rank = 0;
            MPI_Comm_rank(ugf_comms[i], &comm_rank);
            if (mapped && instance["write_final_yaml_to_disk"] &&
                instance["write_final_yaml_to_disk"].as<bool>() &&
                comm_rank == 0) {
                std::ofstream output(
                    driver::KynemaUGF::change_file_name_suffix(
                        logfile, ".yaml"));
                if (!output) throw std::runtime_error("Cannot write UGF YAML");
                output << config;
            }
            sim.register_solver<driver::KynemaUGF>(
                static_cast<int>(i + 1), ugf_comms[i], config, logfile,
                node["ugf_vars"]
                    ? node["ugf_vars"].as<std::vector<std::string>>()
                    : std::vector<std::string>{});
        }
#endif
#ifdef KYNEMA_DRIVER_ENABLE_SGF
        if (sgf_comm != MPI_COMM_NULL) {
            const auto cvars =
                node["sgf_cell_vars"]
                    ? node["sgf_cell_vars"].as<std::vector<std::string>>()
                    : std::vector<std::string>{};
            const auto nvars =
                node["sgf_node_vars"]
                    ? node["sgf_node_vars"].as<std::vector<std::string>>()
                    : std::vector<std::string>{};
            sim.register_solver<driver::KynemaSGF>(
                cvars, nvars, ugf.size() > 0);
        }
#endif
#ifdef KYNEMA_DRIVER_ENABLE_FMB
        for (size_t i = 0; i < fmb.size(); ++i)
            if (fmb_comms[i] != MPI_COMM_NULL)
                sim.register_solver_no_tioga<driver::KynemaFMB>(
                    YAML::LoadFile(fmb[i].as<std::string>()), fmb_comms[i],
                    static_cast<int>(i + 1));
#endif
        sim.initialize();
        sim.run_timesteps(picard, nonlinear, steps, max_time);
        sim.delete_solvers();
    }
#ifdef KYNEMA_DRIVER_ENABLE_SGF
    if (sgf_comm != MPI_COMM_NULL) driver::KynemaSGF::finalize();
#endif
#ifdef KYNEMA_DRIVER_ENABLE_UGF
    if (has_ugf) driver::KynemaUGF::finalize();
#endif
#ifdef KYNEMA_DRIVER_ENABLE_FMB
    if (has_fmb && Kokkos::is_initialized()) Kokkos::finalize();
#endif
    if (sgf_comm != MPI_COMM_NULL) MPI_Comm_free(&sgf_comm);
    free_comms(ugf_comms);
    free_comms(fmb_comms);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
#ifdef KYNEMA_DRIVER_ENABLE_ROCM
    hipInit(0);
#endif
    MPI_Init(&argc, &argv);
    int size = 0, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    try {
        const int result = run(argc, argv, size, rank);
        MPI_Finalize();
        return result;
    } catch (const std::exception& error) {
        std::cerr << "Rank " << rank << ": " << error.what() << '\n';
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }
}
