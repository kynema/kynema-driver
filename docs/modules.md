# Module selection and execution

The driver coordinator is `driver::Simulation` in `src/Simulation.h`.
The shared module interface is `driver::Module` in `src/Module.h`.
Module implementations and the SGF Tioga adapter live in `src/modules/`.
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

Modules execute in the same timestep loop. SGF/UGF overset exchange is
performed when multiple CFD modules are selected. FMB currently runs alongside
the other modules but does not yet exchange forces or displacements with them.
