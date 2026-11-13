#include "src/modules/KynemaFMB.h"
#include "src/Simulation.h"
#include "interfaces/generalized/generalized_structure_interface.hpp"

#include <Kokkos_Core.hpp>
#include <mpi.h>
#include <yaml-cpp/yaml.h>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void run_test(const char* input_file)
{
    using Interface = kynema_fmb::interfaces::GeneralStructureInterface;
    const YAML::Node input = YAML::LoadFile(input_file);
    driver::Simulation simulation(MPI_COMM_WORLD);
    auto& solver = simulation.register_solver_no_tioga<driver::KynemaFMB>(
        input, MPI_COMM_WORLD);
    simulation.initialize();

    Interface reference(input);
    const auto description = reference.initialize_module();
    const auto num_nodes = description["model"]["num_nodes"].as<size_t>();
    const auto mass_node =
        description["components"][0]["mass_node_id"].as<size_t>();
    if (solver.num_nodes() != num_nodes || mass_node >= num_nodes) {
        throw std::runtime_error("FMB model description does not match");
    }

    Interface::HostView<double* [6]> forces("forces", num_nodes);
    Kokkos::deep_copy(forces, 0.);
    std::vector<std::array<double, 6>> driver_forces(num_nodes);
    const double dt = input["simulation"]["step_size"].as<double>();
    for (int step = 0; step < 40; ++step) {
        if (step > 20) {
            forces(mass_node, 2) = 50.;
            driver_forces[mass_node][2] = 50.;
        }
        solver.set_forces(driver_forces);

        reference.predict_inputs();
        reference.advance_states();
        reference.receive_inputs(forces);
        reference.reset_states();
        reference.advance_states();
        reference.calculate_outputs();

        simulation.run_timesteps(0, 2, 1);

        const auto& expected = reference.get_outputs().x;
        for (size_t node = 0; node < num_nodes; ++node) {
            const auto actual = solver.node_position(node);
            for (size_t component = 0; component < actual.size(); ++component) {
                if (!std::isfinite(actual[component]) ||
                    !std::isfinite(expected(node, component)) ||
                    std::abs(actual[component] - expected(node, component)) >
                        1.e-10) {
                    throw std::runtime_error(
                        "FMB state mismatch at step " + std::to_string(step) +
                        ", node " + std::to_string(node) + ", component " +
                        std::to_string(component));
                }
            }
        }
        reference.finalize_time_step();
        if (solver.time_index() != step + 1 ||
            std::abs(solver.call_get_time() - (step + 1) * dt) > 1.e-10) {
            throw std::runtime_error("FMB timestep bookkeeping mismatch");
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);
    Kokkos::initialize(argc, argv);
    int result = 0;
    try {
        if (argc != 2) throw std::runtime_error("Expected FMB input file");
        run_test(argv[1]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    Kokkos::finalize();
    MPI_Finalize();
    return result;
}
