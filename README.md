# helios-thesis

## Reproducing the tagged module versions

`thesis-scenarios/CMakeLists.txt` takes each `helios-<name>` module from `<HELIOS_WORKSPACE_DIR>/helios-<name>` if that directory contains a `CMakeLists.txt`; otherwise it fetches the module from GitHub. `HELIOS_WORKSPACE_DIR` defaults to the directory above this repository. The helios modules resolve their own dependencies (`helios-core`, `helios-math`) in the same way, as sibling directories.

To build exactly the versions used in the thesis, clone this repository and all modules at the tag `master-thesis` into one workspace directory, so that no module is fetched from `main`:

```bash
mkdir helios-workspace && cd helios-workspace

for repo in helios-thesis helios-core helios-math helios-ecs helios-engine \
            helios-gameplay helios-opengl helios-glfw helios-imgui helios-physics; do
  git clone --branch master-thesis "https://github.com/thorstensuckow/${repo}.git"
done
```

Resulting layout:

```
helios-workspace/
  helios-thesis/        this repository
  helios-core/
  helios-math/
  helios-ecs/
  helios-engine/
  helios-gameplay/
  helios-opengl/
  helios-glfw/
  helios-imgui/
  helios-physics/
```

`-DHELIOS_WORKSPACE_DIR=<path>` overrides the workspace directory.

## Note on V7 and V8

The measurement suites `ablationrun_v5_dataparallel` (V7) and `ablationrun_go_prt_parallel` (V8) were each recorded in a separate measurement session. In the data, both carry the variant name `v6`: `variants=v6` and `variants_built=v6` in each `suite.txt`, data directories `n_*/v6/run_*`, build logs `build/v6.log`.

The corresponding source code is `thesis-scenarios/src/v7/` and `thesis-scenarios/src/v8/`. `v7_main.cpp` and `v8_main.cpp` use `GridCollisionDetection` with `THRESHOLD = 10000`, `CHUNK_COUNT = 4` and `USE_THREADS = true`; `v6_main.cpp` uses `grid<UniHandle>()` without threads. The assignment of suite to source follows the thesis (appendix, control measurements for V7 and V8); it cannot be derived from the data alone.

## Scripts

All scripts are in `thesis-scenarios/scripts/`.

| Script | Purpose |
|---|---|
| `run-suite.sh`, `run-suite.ps1` | Run a measurement suite (bash / PowerShell): builds the variants and runs all load scenarios; output goes to `thesis-measurements/suites/<suite-id>/`. Parameters are listed at the top of each script. |
| `measure-build.ps1` | Measure build times of the variants (Ninja logs). |
| `measure-size.ps1` | Measure binary sizes of the variants. |
| `evaluation/evaluate_all.py` | Evaluate a suite: checks that every entry in `summary.csv` has status `OK` and that every time series has the expected number of rows, then calls the scripts below. |
| `evaluation/verify_counters_and_evaluate_means.py` | Counter values per run to `simulation_counter_means.csv`. |
| `evaluation/verify_runtime_results.py` | Result comparison between variants to `simulation_runtime_differences.csv`. |
| `evaluation/evaluate_timing_means.py` | Schedule runtimes to `timing_means.csv`. |
| `evaluation/evaluate_system_means.py` | Per-system runtimes to `system_means.csv`. |
| `evaluation/evaluate_collision_phases.py` | Runtimes of the collision systems to `collision_phase_means.csv`. |
| `plots/make_plots.py` | Create the figures of the thesis from the evaluation CSVs into `<suite>/plots/`. |
| `plots/check_plot_values.py` | Compare the plotted values with the evaluation CSVs. |
| `build/compile-times.py` | Compilation times from Ninja logs (console output). |
| `build/map-count.py` | Evaluate linker map files (console output). |
| `codemetrics/code_metrics_systems.py` | Code metrics of `src/lib/systems` (requires `cloc` and `lizard`). |