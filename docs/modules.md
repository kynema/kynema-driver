# Module selection and execution

The driver coordinator is `driver::Simulation` in `src/Simulation.h`.
The shared module interface is `driver::Module` in `src/Module.h`.
Register modules through `register_module()` (Tioga adapters) or
`register_module_no_tioga()` (native FMB). Initialization hooks use
`prepare_module_prolog()` and `prepare_module_epilog()`.
Every module implements `reset_states()`, called before advancing subsequent
nonlinear iterations. SGF and UGF currently keep their existing state handling
and implement this hook as a no-op.
Driver module adapters live in `src/modules/`. SGF owns its Tioga bridge in
`src/overset/SGFTiogaIface.H` and requires Tioga when built.
Include driver headers with their `src/` paths to distinguish the driver's
`Simulation.h` from the UGF library header of the same name.

Select modules in the `kynema_driver` YAML mapping. Existing SGF and UGF
configuration keys remain supported. FMB accepts either one input filename or
a list:

```yaml
kynema_driver:
  kynema_fmb_inp:
    - structure-a.yaml
    - structure-b.yaml
  fmb_procs: [1, 2]
  num_timesteps: 40
  nonlinear_iterations: 2
```

Input filenames are resolved relative to the working directory. SGF uses
`kynema_sgf_inp`; UGF uses `kynema_ugf_inp` and optionally `ugf_procs`. The
UGF list also accepts the existing `base_input_file`, `replace`, and `logfile`
mapping form. Module keys may be combined in one input file.

Run with MPI, optionally specifying the number of ranks for each module type:

```sh
mpiexec -n 4 kynema_driver --fmb 3 input.yaml
```

The options are `--sgf`, `--ugf`, and `--fmb`. Each selected type defaults
to all ranks. SGF uses ranks starting at zero; UGF and FMB each use a
contiguous range ending at the last rank. Different types may overlap.
Instances of the same type partition its range. If `ugf_procs` or `fmb_procs`
is supplied, its positive counts must sum to the rank count for that type;
otherwise ranks are divided as evenly as possible.

CMake options `KYNEMA_DRIVER_ENABLE_SGF` and `KYNEMA_DRIVER_ENABLE_UGF`
default to ON; `KYNEMA_DRIVER_ENABLE_FMB` defaults to OFF. Set an option to
OFF to build without that module's driver dependency, or enable FMB and
provide `KynemaFMB_DIR` to use it. Tioga and MPI remain driver dependencies.
A YAML file selecting a module disabled in the build is an error.

FMB's `GeneralStructureInterface` implements `driver::Module` directly;
the driver header `src/modules/KynemaFMB.h` is only a type alias, not a
timestepping wrapper. FMB always builds this interface, including for standalone
use, and requires MPI. Its build embeds the shared header-only interface in the
installation without a dependency on the driver library.
`KYNEMA_FMB_DRIVER_MODULE_DIR` only locates these headers; it defaults to the
sibling `kynema-driver/src` directory and is not an integration toggle.
SGF and UGF still use adapters pending refactoring in their respective repositories.

Modules execute in the same timestep loop. Overset hooks are optional no-ops in
`Module`; only CFD modules opt in with `is_cfd()` and implement them.
Overset activates only with at least two CFD modules or instances, counted
once per module communicator, regardless of rank count or non-CFD modules.
Connectivity and overset exchange skip non-CFD modules. SGF/UGF still run their
pre-advance preparation when overset is disabled.
FMB currently runs alongside the other modules but does not yet exchange forces
or displacements with them.
