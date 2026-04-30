#include "KynemaSGF.h"
#include "KynemaUGF.h"
#include "OversetSimulation.h"
#include "MPIUtilities.h"
#include "mpi.h"
#include "yaml-editor.h"
#include "yaml-cpp/yaml.h"
#include "tioga.h"

// Workaround for MPI issue on OLCF Frontier machine
#ifdef KYNEMA_DRIVER_ENABLE_ROCM
#include <hip/hip_runtime.h>
#endif

static std::string usage(std::string name)
{
    return "usage: " + name + " [--sgf NPROCS] [--ugf NPROCS] input_file\n" +
           "\t-h,--help\t\tShow this help message\n" +
           "\t--sgf NPROCS\t\tNumber of ranks for Kynema-SGF (default = all "
           "ranks)\n" +
           "\t--ugf NPROCS\t\tNumber of ranks for Kynema-UGF (default = all "
           "ranks)\n";
}

std::string
replace_extension(const std::string& filepath, const std::string& newExt)
{
    size_t lastDotPos = filepath.find_last_of(".");

    if (lastDotPos != std::string::npos && lastDotPos != 0) {
        return filepath.substr(0, lastDotPos) + newExt;
    } else {
        return filepath + newExt;
    }
}

int main(int argc, char** argv)
{
// Workaround for MPI issue on OLCF Frontier machine
#ifdef KYNEMA_DRIVER_ENABLE_ROCM
    hipInit(0);
#endif
    MPI_Init(&argc, &argv);
    int psize, prank;
    MPI_Comm_size(MPI_COMM_WORLD, &psize);
    MPI_Comm_rank(MPI_COMM_WORLD, &prank);

    if ((argc != 2) && (argc != 4) && (argc != 6)) {
        throw std::runtime_error(usage(argv[0]));
    }

    int num_ugf_ranks = psize;
    int num_sgf_ranks = psize;
    std::string inpfile = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "--help")) {
            if (prank == 0) std::cout << usage(argv[0]);
            return 0;
        } else if (arg == "--sgf") {
            if (i + 1 < argc) {
                std::string opt = argv[++i];
                num_sgf_ranks = std::stoi(opt);
                if (num_sgf_ranks > psize) {
                    throw std::runtime_error(
                        "--sgf option requesting more ranks than available.");
                }
            } else {
                throw std::runtime_error("--sgf option requires one argument.");
            }
        } else if (arg == "--ugf") {
            if (i + 1 < argc) {
                std::string opt = argv[++i];
                num_ugf_ranks = std::stoi(opt);
                if (num_ugf_ranks > psize) {
                    throw std::runtime_error(
                        "--ugf option requesting more ranks than available.");
                }
            } else {
                throw std::runtime_error("--ugf option requires one argument.");
            }
        } else {
            inpfile = argv[i];
        }
    }

    const YAML::Node doc(YAML::LoadFile(inpfile));
    const YAML::Node node = doc["kynema_driver"];
    std::string sgf_inp = "dummy";
    bool use_kynema_sgf = false;
    if (node["kynema_sgf_inp"]) {
        sgf_inp = node["kynema_sgf_inp"].as<std::string>();
        use_kynema_sgf = true;
    }

    const std::string sgf_log = replace_extension(sgf_inp, ".log");
    std::ofstream out;

    YAML::Node kynema_ugf_node = node["kynema_ugf_inp"];
    // make sure it is a list for now
    assert(kynema_ugf_node.IsSequence());
    const int num_ugf_solvers = kynema_ugf_node.size();
    if (num_ugf_ranks < num_ugf_solvers) {
        throw std::runtime_error(
            "Number of Kynema-UGF ranks is less than the number of Kynema-UGF "
            "solvers. Please have at least one rank per solver.");
    }
    std::vector<int> num_ugf_solver_ranks;
    if (node["ugf_procs"]) {
        num_ugf_solver_ranks = node["ugf_procs"].as<std::vector<int>>();
        if (static_cast<int>(num_ugf_solver_ranks.size()) != num_ugf_solvers) {
            throw std::runtime_error(
                "Number of Kynema-UGF rank specifications is less than the "
                " number of Kynema-UGF solvers. Please have one rank count "
                "specification per solver");
        }
        const int tot_num_ugf_ranks = std::accumulate(
            num_ugf_solver_ranks.begin(), num_ugf_solver_ranks.end(), 0);
        if (tot_num_ugf_ranks != num_ugf_ranks) {
            throw std::runtime_error(
                "Total number of Kynema-UGF ranks does not "
                "match that given in the command line. Please ensure "
                "they match");
        }
    } else {
        const int ranks_per_ugf_solver = num_ugf_ranks / num_ugf_solvers;
        num_ugf_solver_ranks =
            std::vector<int>(num_ugf_solvers, ranks_per_ugf_solver);
        const int remainder = num_ugf_ranks % num_ugf_solvers;
        if (remainder != 0) {
            std::fill(
                num_ugf_solver_ranks.begin() + num_ugf_solvers - remainder,
                num_ugf_solver_ranks.end(), ranks_per_ugf_solver + 1);
        }
    }

    if (!use_kynema_sgf) {
        num_sgf_ranks = 0;
    }

    if (num_sgf_ranks + num_ugf_ranks < psize) {
        if (prank == 0)
            throw std::runtime_error(
                "Abort: using fewer ranks than available ranks: MPI "
                "size = " +
                std::to_string(psize) + "; Num ranks used = " +
                std::to_string(num_sgf_ranks + num_ugf_ranks));
    }

    MPI_Comm sgf_comm =
        use_kynema_sgf
            ? driver::create_subcomm(MPI_COMM_WORLD, num_sgf_ranks, 0)
            : MPI_COMM_NULL;

    std::vector<MPI_Comm> kynema_ugf_comms;
    std::vector<int> kynema_ugf_start_rank;
    int start = psize - num_ugf_ranks;
    for (const auto& nr : num_ugf_solver_ranks) {
        kynema_ugf_start_rank.push_back(start);
        kynema_ugf_comms.push_back(
            driver::create_subcomm(MPI_COMM_WORLD, nr, start));
        start += nr;
    }

    driver::OversetSimulation sim(MPI_COMM_WORLD);
    if (sgf_comm != MPI_COMM_NULL) {
        sim.echo(
            "Initializing Kynema-SGF on " + std::to_string(num_sgf_ranks) +
            " MPI ranks");
        out.open(sgf_log);
        driver::KynemaSGF::initialize(sgf_comm, sgf_inp, out);
    }
    sim.echo(
        "Initializing " + std::to_string(num_ugf_solvers) +
        " Kynema-UGF solvers, equally partitioned on a total of " +
        std::to_string(num_ugf_ranks) + " MPI ranks");
    if (std::any_of(
            kynema_ugf_comms.begin(), kynema_ugf_comms.end(),
            [](const auto& comm) { return comm != MPI_COMM_NULL; })) {
        driver::KynemaUGF::initialize();
    }
    sim.set_ugf_start_rank(kynema_ugf_start_rank);

    const auto ugf_vars = node["ugf_vars"].as<std::vector<std::string>>();
    const int num_timesteps =
        node["num_timesteps"] ? node["num_timesteps"].as<int>() : -1;
    const double max_time =
        node["max_time"] ? node["max_time"].as<double>() : -1.0;
    const int additional_picard_its =
        node["additional_picard_iterations"]
            ? node["additional_picard_iterations"].as<int>()
            : 0;
    const int nonlinear_its = node["nonlinear_iterations"]
                                  ? node["nonlinear_iterations"].as<int>()
                                  : 1;
    const bool holemap_alg = node["use_adaptive_holemap"]
                                 ? node["use_adaptive_holemap"].as<bool>()
                                 : false;
    sim.set_holemap_alg(holemap_alg);

    if (num_timesteps < 0 && max_time < 0.) {
        throw std::runtime_error(
            "max_timesteps or num_timesteps must be specified as positive "
            "values. These are both unspecified or specified as negative.");
    }

    if (node["composite_body"]) {
        const YAML::Node& composite_mesh = node["composite_body"];
        const int num_composite = static_cast<int>(composite_mesh.size());
        sim.set_composite_num(num_composite);

        for (int i = 0; i < num_composite; i++) {
            const YAML::Node& composite_node = composite_mesh[i];

            const int num_body_tags = composite_node["num_body_tags"].as<int>();

            const auto body_tags =
                composite_node["body_tags"].as<std::vector<int>>();

            const auto dominance_tags =
                composite_node["dominance_tags"].as<std::vector<int>>();

            const double search_tol =
                composite_node["search_tolerance"].as<double>();

            sim.set_composite_body(
                i, num_body_tags, body_tags, dominance_tags, search_tol);
        }
    }

    const YAML::Node yaml_replace_all = node["ugf_replace_all"];
    for (int i = 0; i < num_ugf_solvers; i++) {
        if (kynema_ugf_comms.at(i) != MPI_COMM_NULL) {
            YAML::Node yaml_replace_instance;
            YAML::Node this_instance = kynema_ugf_node[i];

            std::string kynema_ugf_inpfile, logfile;
            bool write_final_yaml_to_disk = false;
            if (this_instance.IsMap()) {
                yaml_replace_instance = this_instance["replace"];
                kynema_ugf_inpfile =
                    this_instance["base_input_file"].as<std::string>();
                // deal with the logfile name
                if (this_instance["logfile"]) {
                    logfile = this_instance["logfile"].as<std::string>();
                } else {
                    logfile = driver::KynemaUGF::change_file_name_suffix(
                        kynema_ugf_inpfile, ".log", i);
                }
                if (this_instance["write_final_yaml_to_disk"]) {
                    write_final_yaml_to_disk =
                        this_instance["write_final_yaml_to_disk"].as<bool>();
                }

            } else {
                kynema_ugf_inpfile = this_instance.as<std::string>();
                logfile = driver::KynemaUGF::change_file_name_suffix(
                    kynema_ugf_inpfile, ".log");
            }

            YAML::Node kynema_ugf_yaml = YAML::LoadFile(kynema_ugf_inpfile);
            // replace in order so instance can overwrite all
            if (yaml_replace_all) {
                YEDIT::find_and_replace(kynema_ugf_yaml, yaml_replace_all);
            }
            if (yaml_replace_instance) {
                YEDIT::find_and_replace(kynema_ugf_yaml, yaml_replace_instance);
            }

            // only the first rank of the comm should write the file
            int comm_rank = -1;
            MPI_Comm_rank(kynema_ugf_comms.at(i), &comm_rank);
            if (write_final_yaml_to_disk && comm_rank == 0) {
                auto new_ifile_name =
                    driver::KynemaUGF::change_file_name_suffix(
                        logfile, ".yaml");
                std::ofstream fout(new_ifile_name);
                fout << kynema_ugf_yaml;
                fout.close();
            }

            sim.register_solver<driver::KynemaUGF>(
                i + 1, kynema_ugf_comms.at(i), kynema_ugf_yaml, logfile,
                ugf_vars);
        }
    }

    if (sgf_comm != MPI_COMM_NULL) {
        const auto sgf_cvars =
            node["sgf_cell_vars"].as<std::vector<std::string>>();
        const auto sgf_nvars =
            node["sgf_node_vars"].as<std::vector<std::string>>();

        sim.register_solver<driver::KynemaSGF>(sgf_cvars, sgf_nvars);
    }

    sim.echo("Initializing overset simulation");
    sim.initialize();
    sim.echo("Initialization successful");
    sim.run_timesteps(
        additional_picard_its, nonlinear_its, num_timesteps, max_time);
    sim.delete_solvers();

    if (sgf_comm != MPI_COMM_NULL) {
        driver::KynemaSGF::finalize();
        out.close();
    }
    if (std::any_of(
            kynema_ugf_comms.begin(), kynema_ugf_comms.end(),
            [](const auto& comm) { return comm != MPI_COMM_NULL; })) {
        driver::KynemaUGF::finalize();
    }
    MPI_Finalize();

    return 0;
}
