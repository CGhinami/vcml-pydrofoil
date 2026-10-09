# Benchmarks

| Benchmark | Config | ELFs |
|---|---|---|
| Zephyr SMP, NAS IS | `zephyr_is/zephyr_is.cfg` | `zephyr_is/build/zephyr_is_<class>_<n>cores.elf`, built by `zephyr_tests/build_is.sh` |
| Dhrystone, one instance per core (baremetal) | `dhrystone_multicore/dhrystone.cfg` | `dhrystone_multicore/build/dhrystone_core<i>.riscv`, built by riscv-tests (`multicore_test/dhrystone`) |

Every config runs on its own and takes overrides after it:

```bash
./launch.sh benchmark/zephyr_is/zephyr_is.cfg -c system.ncores=4 \
    -c elf=benchmark/zephyr_is/build/zephyr_is_W_4cores.elf -c system.quantum=100us
```

## Sweeps

`benchmark.py` runs every combination of the parameters in a sweep file and
appends one line per run to `results/<sweep>.csv` (the log of each run goes to
`results/logs/<sweep>/`):

```bash
python3 benchmark/benchmark.py benchmark/sweeps/smoke.json --dry-run   # list the runs
python3 benchmark/benchmark.py benchmark/sweeps/zephyr_is.json
```

```json
[{"benchmark": "zephyr_is", "runs": 3, "timeout": 1800,
  "class": ["S", "W"], "n_cores": [1, 2, 4, 8], "quantum": ["1us", "100us"],
  "async": [false, true], "async_rate": [1, 5, 10]}]
```

- `async_rate` only applies to `async=true`.
- Runs already in the CSV are skipped, an interrupted sweep continues where it
  stopped. `--retry-failed` runs again what did not finish with `ok`.
- `status`: `ok`, `timeout` (killed after `timeout` seconds), `crash` (non-zero
  exit), `unfinished` (ended without all cores reporting done, e.g.
  `system.duration`), `missing_elf`.
- `benchmark_time` is the wall clock time since the guest read
  `multicore_simdev.hclk` (start of the measured part, Zephyr IS only),
  `runtime` the wall clock time of the whole simulation, `duration` the
  simulated time. `vp_commit` is the VP version the run was made with
  (`-dirty` when `sysc_vp/` or `gluecode.py` had uncommitted changes).
- Zephyr IS on several cores with `async=true` is not reliable before the
  atomics are handled in Pydrofoil (runs can hang, see the timeout).

## Plots

```bash
pip install pandas matplotlib seaborn
python3 benchmark/plot.py results/zephyr_is.csv                 # benchmark_time
python3 benchmark/plot.py results/zephyr_is.csv --metric mips
```

One figure per benchmark and IS class in `results/plots/`: cores on the x
axis, colour = quantum, marker = sync / async rate, mean and standard
deviation over the iterations.
