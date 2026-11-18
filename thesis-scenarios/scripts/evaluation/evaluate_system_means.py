#!/usr/bin/env python3
"""Mean of the run means of the system and manager timings per n and variant (update, flush and total).

For each run, the time series of a system/manager (<name>_timings.csv) and its flush() time series
(<name>_flush_timings.csv; a missing file means the column was constant 0) are averaged over all frames of the
measurement phase; the run means are then averaged over the runs. total = update + flush (per run).
The group series p_det/p_res are omitted (they include flush()).

Series that do not exist for a variant (e.g. s_delta_*_prt in V1/V2, which use a single combined system stored
in the *_go series) are left empty. The only check: for every n, the same variants and the same number of runs
must exist.

Layout (relative to this script):
    <root>/<suite>/n_<n>/v<k>/run_<id>/*.csv
Output: <suite>/system_means.csv (long format: n, variant, system, runs, update_/flush_/total_ mean, min and max
of the run means in ns and us)
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import DEFAULT_ROOT, check_layout, discover_layout, resolve_suite  # noqa: E402

# Name in the output -> CSV file in the run directory (columns: frame, <time in ns>)
SYSTEMS = {
    "m_col": "m_col_timings",
    "m_lft": "m_lft_timings",
    "s_beta": "s_beta_timings",
    "s_delta_det_go": "s_delta_det_go_timings",
    "s_delta_det_prt": "s_delta_det_prt_timings",
    "s_delta_res_go": "s_delta_res_go_timings",
    "s_delta_res_prt": "s_delta_res_prt_timings",
    "s_lambda_prt": "s_lambda_prt_timings",
}
OUTPUT_NAME = "system_means.csv"


def run_mean_ns(run_dir: Path, file_name: str) -> float | None:
    """Mean of a time series [ns] over all frames of a run; None if the file does not exist."""
    path = run_dir / f"{file_name}.csv"
    if not path.exists():
        return None
    return float(pd.read_csv(path, usecols=[1]).iloc[:, 0].mean())


def run(suite: Path, only_n: int | None = None, output: Path | None = None) -> Path:
    """Write system_means.csv for the suite directory; returns the output path."""
    layout = discover_layout(suite, only_n)
    check_layout(layout)   # only check: same variants and same number of runs for every n

    rows = []
    for n, variants in sorted(layout.items()):
        for variant, runs in variants.items():
            for system, file_name in SYSTEMS.items():
                values = [run_mean_ns(r, file_name) for r in runs]
                if any(v is None for v in values):
                    continue   # series does not exist for this variant
                # flush() time of the same run; a missing file means the column was constant 0
                flush = [run_mean_ns(r, f"{system}_flush_timings") or 0.0 for r in runs]
                update_runs = pd.Series(values)
                flush_runs = pd.Series(flush)
                total_runs = update_runs + flush_runs
                rows.append({
                    "n": n, "variant": variant, "system": system, "runs": len(runs),
                    "update_mean_ns": update_runs.mean(), "update_mean_us": update_runs.mean() / 1000.0,
                    "update_min_run_mean_us": update_runs.min() / 1000.0,
                    "update_max_run_mean_us": update_runs.max() / 1000.0,
                    "flush_mean_ns": flush_runs.mean(), "flush_mean_us": flush_runs.mean() / 1000.0,
                    "flush_min_run_mean_us": flush_runs.min() / 1000.0,
                    "flush_max_run_mean_us": flush_runs.max() / 1000.0,
                    "total_mean_ns": total_runs.mean(), "total_mean_us": total_runs.mean() / 1000.0,
                    "total_min_run_mean_us": total_runs.min() / 1000.0,
                    "total_max_run_mean_us": total_runs.max() / 1000.0,
                })

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
