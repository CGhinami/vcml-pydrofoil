#!/usr/bin/env python3
"""Run a benchmark sweep on the VP and collect the results in a CSV file.

    python3 benchmark/benchmark.py benchmark/sweeps/zephyr_is.json
    python3 benchmark/benchmark.py benchmark/sweeps/smoke.json --dry-run

A sweep file describes one or more blocks of parameters; every combination of
the listed values is run 'runs' times:

    [{"benchmark": "zephyr_is", "runs": 3, "timeout": 600,
      "class": ["S", "W"], "n_cores": [1, 2, 4, 8],
      "quantum": ["1us", "100us"], "async": [false, true], "async_rate": [1, 5]}]

async_rate only matters with async=true, for async=false it is left empty.
Results are appended to results/<sweep name>.csv (or --out), runs that are
already in that file are skipped, so an interrupted sweep can be restarted.
The output of every run is kept in results/logs/.
"""

import argparse
import csv
import datetime
import itertools
import json
import os
import re
import signal
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAX_HARTS = 8

# Base config per benchmark; the sweep parameters are applied on top with -c
BENCHMARKS = {
    "zephyr_is": {
        "cfg": "benchmark/zephyr_is/zephyr_is.cfg",
        "elf": "benchmark/zephyr_is/build/zephyr_is_{cls}_{n_cores}cores.elf",
    },
    "dhrystone": {
        "cfg": "benchmark/dhrystone_multicore/dhrystone.cfg",
        "elf": None,  # fixed set of per-core ELFs in the config
    },
}

PARAMS = ["benchmark", "class", "n_cores", "quantum", "async", "async_rate"]
RESULTS = ["status", "exit_code", "wall_time", "duration", "runtime",
           "benchmark_time", "instructions", "mips"]
HARTS = ["insns_hart%d" % i for i in range(MAX_HARTS)]
COLUMNS = ["timestamp", "vp_commit"] + PARAMS + ["iteration"] + RESULTS + HARTS + ["log"]

# Statistics printed by system::run() at the end of every simulation
PATTERNS = {
    "duration": re.compile(r"\]\s+duration\s*:\s*([\d.]+)s"),
    "runtime": re.compile(r"\]\s+runtime\s*:\s*([\d.]+)s"),
    "benchmark_time": re.compile(r"\]\s+benchmark time\s*:\s*([\d.]+)s"),
    "instructions": re.compile(r"\]\s+instructions\s*:\s*(\d+)"),
    "mips": re.compile(r"\]\s+sim speed\s*:\s*([\d.]+)\s*MIPS"),
}
HART_PATTERN = re.compile(r"\]\s+hart (\d+)\s*:\s*(\d+)")
FINISHED = "cores have finished"


def expand(block):
    """All parameter combinations of one sweep block."""
    bench = block["benchmark"]
    if bench not in BENCHMARKS:
        sys.exit("unknown benchmark '%s', known: %s" % (bench, ", ".join(BENCHMARKS)))

    classes = block.get("class", [""]) if BENCHMARKS[bench]["elf"] else [""]
    seen = set()
    for cls, n, q, a, rate in itertools.product(classes, block["n_cores"], block["quantum"],
                                                block.get("async", [False]),
                                                block.get("async_rate", [5])):
        run = {"benchmark": bench, "class": cls, "n_cores": str(n), "quantum": q,
               "async": str(bool(a)).lower(), "async_rate": str(rate) if a else ""}
        key = tuple(run[p] for p in PARAMS)
        if key not in seen:
            seen.add(key)
            yield run


def vp_args(run):
    """sysc_vp arguments that turn the base config into this run."""
    n = int(run["n_cores"])
    args = ["-c", "system.ncores=%d" % n, "-c", "system.quantum=%s" % run["quantum"]]
    for i in range(n):
        args += ["-c", "system.core%d.async=%s" % (i, run["async"])]
        if run["async_rate"]:
            args += ["-c", "system.core%d.async_rate=%s" % (i, run["async_rate"])]
    elf = BENCHMARKS[run["benchmark"]]["elf"]
    if elf:
        args += ["-c", "elf=" + elf.format(cls=run["class"], n_cores=n)]
    return args


def vp_commit():
    try:
        commit = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=REPO,
                                         text=True).strip()
        dirty = subprocess.call(["git", "diff", "--quiet", "HEAD", "--", "sysc_vp", "gluecode.py"],
                                cwd=REPO)
        return commit + ("-dirty" if dirty else "")
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def parse(output):
    res = {}
    for name, pattern in PATTERNS.items():
        m = pattern.search(output)
        res[name] = m.group(1) if m else ""
    for hart, insns in HART_PATTERN.findall(output):
        res["insns_hart" + hart] = insns
    # Single core: the VP prints no per-hart line
    if "insns_hart0" not in res:
        res["insns_hart0"] = res["instructions"]
    return res


def execute(run, iteration, timeout, log_dir):
    cfg = BENCHMARKS[run["benchmark"]]["cfg"]
    args = vp_args(run)
    elf = BENCHMARKS[run["benchmark"]]["elf"]
    if elf:
        path = os.path.join(REPO, elf.format(cls=run["class"], n_cores=run["n_cores"]))
        if not os.path.isfile(path):
            return {"status": "missing_elf", "exit_code": "", "log": path}

    name = "_".join(run[p] or "-" for p in PARAMS) + "_%d" % iteration
    log = os.path.join(log_dir, name + ".log")
    cmd = ["./launch.sh", cfg] + args

    start = time.monotonic()
    with open(log, "w") as f:
        f.write("# " + " ".join(cmd) + "\n")
        f.flush()
        # own process group, so a timeout also stops sysc_vp below launch.sh
        proc = subprocess.Popen(cmd, cwd=REPO, stdout=f, stderr=subprocess.STDOUT,
                                start_new_session=True)
        try:
            rc = proc.wait(timeout=timeout)
            status = None
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            rc = proc.wait()
            status = "timeout"
    wall = time.monotonic() - start

    with open(log, errors="replace") as f:
        output = f.read()
    res = parse(output)
    if status is None:
        if rc != 0:
            status = "crash"
        elif FINISHED not in output:
            status = "unfinished"  # e.g. system.duration reached
        else:
            status = "ok"
    res.update({"status": status, "exit_code": rc, "wall_time": "%.2f" % wall,
                "log": os.path.relpath(log, REPO)})
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sweep", help="sweep description (JSON)")
    ap.add_argument("--out", help="CSV file (default: results/<sweep name>.csv)")
    ap.add_argument("--retry-failed", action="store_true",
                    help="run again what is in the CSV with a status other than ok")
    ap.add_argument("--dry-run", action="store_true", help="only list the runs")
    opts = ap.parse_args()

    with open(opts.sweep) as f:
        blocks = json.load(f)
    if isinstance(blocks, dict):
        blocks = [blocks]

    sweep_name = os.path.splitext(os.path.basename(opts.sweep))[0]
    out = opts.out or os.path.join(REPO, "results", sweep_name + ".csv")
    log_dir = os.path.join(os.path.dirname(os.path.abspath(out)), "logs", sweep_name)

    done = set()
    if os.path.isfile(out):
        with open(out, newline="") as f:
            for row in csv.DictReader(f):
                if row["status"] == "ok" or not opts.retry_failed:
                    done.add(tuple(row[p] for p in PARAMS) + (row["iteration"],))

    todo = []
    for block in blocks:
        for run in expand(block):
            for it in range(1, block.get("runs", 1) + 1):
                if tuple(run[p] for p in PARAMS) + (str(it),) not in done:
                    todo.append((run, it, block.get("timeout", 600)))

    print("%d runs to do, results in %s" % (len(todo), os.path.relpath(out)))
    if opts.dry_run:
        for run, it, _ in todo:
            print("  %s  #%d  ./launch.sh %s %s" % (" ".join("%s=%s" % (p, run[p]) for p in PARAMS[1:] if run[p]),
                                                  it, BENCHMARKS[run["benchmark"]]["cfg"], " ".join(vp_args(run))))
        return

    os.makedirs(log_dir, exist_ok=True)
    if not os.path.isfile(out):
        with open(out, "w", newline="") as f:
            csv.writer(f).writerow(COLUMNS)

    commit = vp_commit()
    for k, (run, it, timeout) in enumerate(todo, 1):
        desc = " ".join("%s=%s" % (p, run[p]) for p in PARAMS if run[p])
        print("[%d/%d] %s #%d ... " % (k, len(todo), desc, it), end="", flush=True)
        res = execute(run, it, timeout, log_dir)
        print("%s  %ss  benchmark_time=%s" % (res["status"], res.get("wall_time", "-"),
                                             res.get("benchmark_time", "-") or "-"))
        row = dict(run, iteration=it, timestamp=datetime.datetime.now().isoformat(timespec="seconds"),
                   vp_commit=commit, **res)
        with open(out, "a", newline="") as f:
            csv.DictWriter(f, fieldnames=COLUMNS, extrasaction="ignore").writerow(row)


if __name__ == "__main__":
    main()
