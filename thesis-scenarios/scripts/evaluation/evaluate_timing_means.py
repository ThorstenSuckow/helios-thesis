#!/usr/bin/env python3
"""Mean of the run means of the timing data per n and variant.

For each run, the time series is averaged over all frames of the measurement phase; the run means are then
averaged over the runs. Timing data so far: execution time t_S of the schedule (schedule_timings.csv).
Further time series can be added to TIMINGS.

The only check: for every n, the same variants and the same number of runs must exist.

Layout (relative to this script):
    <root>/<suite>/n_<n>/v<k>/run_<id>/*.csv
Output: <suite>/timing_means.csv (times in ns as measured, and in us)
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import DEFAULT_ROOT, check_layout, discover_layout, resolve_suite  # noqa: E402

# Name in the output -> CSV file in the run directory (columns: frame, <time in ns>)
TIMINGS = {
    "schedule": "schedule_timings",
}
OUTPUT_NAME = "timing_means.csv"


def run_mean_ns(run_dir: Path, file_name: str) -> float:
    """Mean of a time series [ns] over all frames of a run."""
    path = run_dir / f"{file_name}.csv"
    if not path.exists():
        raise FileNotFoundError(path)
    return float(pd.read_csv(path, usecols=[1]).iloc[:, 0].mean())


def run(suite: Path, only_n: int | None = None, output: Path | None = None) -> Path:
    """Write timing_means.csv for the suite directory; returns the output path."""
    layout = discover_layout(suite, only_n)
    check_layout(layout)   # only check: same variants and same number of runs for every n

    rows = []
    for n, variants in sorted(layout.items()):
        for variant, runs in variants.items():
            row = {"n": n, "variant": variant, "runs": len(runs)}
            for name, file_name in TIMINGS.items():
                per_run = pd.Series([run_mean_ns(r, file_name) for r in runs])
                row[f"mean_{name}_ns"] = per_run.mean()
                row[f"mean_{name}_us"] = per_run.mean() / 1000.0
                row[f"min_run_mean_{name}_us"] = per_run.min() / 1000.0
                row[f"max_run_mean_{name}_us"] = per_run.max() / 1000.0
            rows.append(row)

    output = output or suite / OUTPUT_NAME
    pd.DataFrame(rows).to_csv(output, index=False, float_format="%.6f")
    print(f"OK - {len(rows)} rows written to {output}")
    return output


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", required=True, metavar="NAME", help="Suite directory name below --root")
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT, help="Root directory containing the suites")
    parser.add_argument("--n", type=int, help="Only this n")
    parser.add_argument("--output", type=Path, help=f"Output CSV (default: <suite>/{OUTPUT_NAME})")
    args = parser.parse_args()
    run(resolve_suite(args.root, args.suite), args.n, args.output)


if __name__ == "__main__":
    main()
