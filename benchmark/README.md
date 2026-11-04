# Benchmarks

This README covers the Linux NPB benchmark for now. `benchmark.py` also runs
Zephyr IS and Dhrystone, see its help text.

## Linux NPB

The VP boots Linux with 1/2/4/8 harts and runs one benchmark of NPB3.3-OMP
during boot, with one OpenMP thread per hart:

- benchmarks (`app`): `bt`, `cg`, `ep`, `ft`, `is`, `lu`, `mg`, `sp`
- classes (`class`): `S`, `W`, `A`, `B`, plus `C` for `ep` and `is`


### One run by hand

```bash
./launch.sh <Pydrofoil-RISCV-benchmarks>/sw/images/buildroot/pydrofoil.cfg -c system.ncores=4
```

This boots Linux. Without `/etc/npb_job` in the root file system no benchmark
runs, sweeps write that file for every run.

## Sweeps

`benchmark.py` runs every combination of the parameters in a sweep file, `runs`
times each. In the container (VP and Pydrofoil plugins from the image, see the
top-level README):

```bash
./container_run_benchmark.sh podman benchmark/sweeps/linux_npb_fixed_1rep.json \
    --images <Pydrofoil-RISCV-benchmarks>/sw/images/buildroot \
    --out results/linux_npb_fixed.csv
```

Without the container:

```bash
python3 benchmark/benchmark.py benchmark/sweeps/linux_npb.json \
    --images <Pydrofoil-RISCV-benchmarks>/sw/images/buildroot
```

| Option | Meaning |
|---|---|
| `--images` | Image directory, `pydrofoil.cfg` and `rootfs.ext4` are taken from there |
| `--out` | CSV file, default `results/<sweep>.csv` |
| `--dry-run` | Only list the runs |
| `--retry-failed` | Run again what did not finish with `ok` |

A sweep file is a list of blocks:

```json
[{"benchmark": "linux_npb", "runs": 10, "timeout": 900,
  "app": ["cg", "is"], "class": ["S"], "n_cores": [2, 4, 8],
  "quantum": ["100us"], "async": [true], "async_rate": [10],
  "pydrofoil": ["new"]}]
```

- `async_rate` only applies to `async=true`, for `async=false` the column is
  empty. A quantum is split into `async_rate` chunks.
- `pydrofoil` picks the plugin in `pydrofoil/<name>/`, default `old`. `new` has
  the atomics fix that `async=true` needs. `async=false` does not need it, so
  those runs use `old`.
- `timeout` is in seconds. A run that times out is killed and counts as done
  unless `--retry-failed` is given.
- Runs already in the CSV are skipped, so an interrupted sweep continues where
  it stopped. Only the CSV as it is at the start of a sweep counts.

## What one run does

1. `benchmark.py` copies `rootfs.ext4` and writes `/etc/npb_job` into the copy
   (`e2fsck`, `debugfs`), for example `NPB_BENCH=cg.W` and `OMP_NUM_THREADS=4`.
2. The VP boots OpenSBI and Linux, then BusyBox init runs the scripts in
   `/etc/init.d/`.
3. `S98npb` prints the host time, runs the benchmark, and prints the host time
   and the exit code (`NPB_HCLK_START`, `NPB_HCLK_END`, `NPB_EXIT`).
4. `S99hello_world` calls `simexit`, which ends the simulation.
5. `benchmark.py` parses the log and appends one line to the CSV. The log stays
   in `results/logs/<sweep>/`.

## Results

| Column | Meaning |
|---|---|
| `status` | `ok` needs "Verification = SUCCESSFUL". Otherwise `timeout`, `crash` (non-zero exit), `unfinished` (ended without successful verification) or `missing_rootfs` |
| `benchmark_time` | Host seconds of the benchmark alone, `NPB_HCLK_END` - `NPB_HCLK_START` |
| `guest_time` | NPB's "Time in seconds", simulated time of its compute part |
| `runtime` | Host seconds of the whole simulation, including the boot |
| `duration` | Simulated seconds of the whole simulation |
| `mops` | NPB's Mop/s |
| `instructions`, `mips` | Instructions of all harts and simulation speed |
| `max_rss_mb` | Peak host memory of the VP in MiB, empty after a timeout |
| `iteration` | Number of the repetition, starts at 1 |
| `pydrofoil` | Plugin the run used |
| `vp_commit` | VP version, `-dirty` when `sysc_vp/` or `gluecode.py` had uncommitted changes |

Compare speed with `benchmark_time`, it does not contain the boot.


