#!/usr/bin/env python3
"""Plot the results of benchmark/benchmark.py.

    python3 benchmark/plot.py results/zephyr_is.csv
    python3 benchmark/plot.py results/zephyr_is.csv --metric mips --show

One figure per benchmark (and IS class): x = number of cores, y = metric,
colour = quantum, marker = sync / async rate. The mean over the iterations is
plotted with the standard deviation as error bars. Only runs with status ok
are used. Figures go to results/plots/ next to the CSV (or --out-dir).

Needs pandas, matplotlib and seaborn (pip install pandas matplotlib seaborn).
"""

import argparse
import os

import matplotlib
import pandas as pd
import seaborn as sns

LABELS = {
    "benchmark_time": "Benchmark time (s, wall clock)",
    "runtime": "Simulation runtime (s, wall clock)",
    "duration": "Simulated time (s)",
    "mips": "Simulation speed (MIPS)",
    "instructions": "Instructions (all harts)",
}


def mode(row):
    return "sync" if row["async"] is False else "async, rate %d" % row["async_rate"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--metric", default="benchmark_time", choices=list(LABELS),
                    help="benchmark_time falls back to runtime where it is empty (Dhrystone)")
    ap.add_argument("--out-dir", help="default: plots/ next to the CSV")
    ap.add_argument("--show", action="store_true", help="also open the figures")
    opts = ap.parse_args()

    if not opts.show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    df = pd.read_csv(opts.csv, keep_default_na=False, na_values=[""])
    skipped = df[df["status"] != "ok"]
    if len(skipped):
        print("ignoring %d runs that are not ok:" % len(skipped))
        print(skipped.groupby(["benchmark", "status"]).size().to_string())
    df = df[df["status"] == "ok"].copy()
    if df.empty:
        raise SystemExit("no successful runs in " + opts.csv)

    df["async"] = df["async"].astype(str).str.lower() == "true"
    df["async_rate"] = df["async_rate"].fillna(0).astype(int)
    df["mode"] = df.apply(mode, axis=1)
    df["class"] = df["class"].fillna("")

    metric = opts.metric
    if metric == "benchmark_time":
        df["value"] = df["benchmark_time"].fillna(df["runtime"])
    else:
        df["value"] = df[metric]

    # quanta in numeric order, not alphabetical
    units = {"ns": 1e-9, "us": 1e-6, "ms": 1e-3, "s": 1}
    def seconds(q):
        for u in ("ns", "us", "ms", "s"):
            if q.endswith(u):
                return float(q[: -len(u)]) * units[u]
        return float(q)
    quanta = sorted(df["quantum"].unique(), key=seconds)
    modes = ["sync"] + sorted(m for m in df["mode"].unique() if m != "sync")

    out_dir = opts.out_dir or os.path.join(os.path.dirname(os.path.abspath(opts.csv)), "plots")
    os.makedirs(out_dir, exist_ok=True)
    sns.set_theme(style="whitegrid")

    for (bench, cls), data in df.groupby(["benchmark", "class"]):
        title = bench + (" class %s" % cls if cls else "")
        fig, ax = plt.subplots(figsize=(10, 6))
        sns.lineplot(data=data, x="n_cores", y="value", hue="quantum",
                     hue_order=[q for q in quanta if q in set(data["quantum"])],
                     style="mode", style_order=[m for m in modes if m in set(data["mode"])],
                     markers=True, dashes=False, errorbar="sd", err_style="bars",
                     err_kws={"capsize": 4}, markersize=8, linewidth=2, ax=ax)
        ax.set_xticks(sorted(data["n_cores"].unique()))
        ax.set_xlabel("Cores")
        label = LABELS[metric]
        if metric == "benchmark_time" and data["benchmark_time"].isna().all():
            label = LABELS["runtime"]
        ax.set_ylabel(label)
        runs = data.groupby(["n_cores", "quantum", "mode"]).size()
        ax.set_title("%s (%d-%d runs per point)" % (title, runs.min(), runs.max()))
        ax.legend(bbox_to_anchor=(1.02, 1), loc="upper left", borderaxespad=0)
        fig.tight_layout()

        name = "%s%s_%s.png" % (bench, "_" + cls if cls else "", metric)
        path = os.path.join(out_dir, name)
        fig.savefig(path, dpi=200, bbox_inches="tight")
        print("wrote", os.path.relpath(path))

    if opts.show:
        plt.show()


if __name__ == "__main__":
    main()
