#!/usr/bin/env python3
"""Create all evaluation CSVs of a measurement suite.

Usage:
    python scripts/evaluation/evaluate_all.py --suite <name> [--root <dir>] [--n <n>]

Checks first (any failure aborts with exit code 1, nothing is filtered):
    * every row of <suite>/summary.csv has the status OK
    * every time series (<name>_timings.csv, <name>_flush_timings.csv) of every run has exactly
      `measure_frames` data rows (from <suite>/suite.txt)

Then, in this order (written to <suite>/, unchanged names and columns):
    1. verify_counters_and_evaluate_means  -> simulation_counter_means.csv  (H8)
    2. verify_runtime_results              -> simulation_runtime_differences.csv  (H9)
    3. evaluate_system_means               -> system_means.csv
    4. evaluate_timing_means               -> timing_means.csv
    5. evaluate_collision_phases           -> collision_phase_means.csv
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
import evaluate_collision_phases  # noqa: E402
import evaluate_system_means  # noqa: E402
import evaluate_timing_means  # noqa: E402
import verify_counters_and_evaluate_means  # noqa: E402
import verify_runtime_results  # noqa: E402
from common import DEFAULT_ROOT, discover_layout, resolve_suite  # noqa: E402


class CheckError(Exception):
    """A precondition of the evaluation is violated."""


def read_suite_txt(suite: Path) -> dict[str, str]:
    path = suite / "suite.txt"
    if not path.is_file():
        raise CheckError(f"{path} is missing")
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def check_status(suite: Path) -> None:
    """Every run listed in summary.csv must have the status OK."""
    path = suite / "summary.csv"
    if not path.is_file():
        raise CheckError(f"{path} is missing")
    summary = pd.read_csv(path, dtype=str)
    if "status" not in summary.columns:
        raise CheckError(f"{path} has no column 'status'")
    bad = summary[summary["status"] != "OK"]
    if not bad.empty:
        rows = "; ".join(f"n={r.n} {r.variant} run={r.run}: {r.status}" for r in bad.itertuples())
        raise CheckError(f"{len(bad)} run(s) in {path} do not have the status OK: {rows}")
    print(f"Status: {len(summary)} runs, all OK")


def data_rows(path: Path) -> int:
    with path.open("rb") as file:
        lines = sum(1 for _ in file)
    return max(lines - 1, 0)   # header


def check_frames(suite: Path, only_n: int | None) -> None:
    """Every time series must have exactly measure_frames rows."""
    meta = read_suite_txt(suite)
    if "measure_frames" not in meta:
        raise CheckError(f"measure_frames missing in {suite / 'suite.txt'}")
    expected = int(meta["measure_frames"])
    checked = 0
    wrong = []
    for n, variants in sorted(discover_layout(suite, only_n).items()):
        for variant, runs in variants.items():
            for run_dir in runs:
                for path in sorted(run_dir.glob("*timings.csv")):
                    if path.name.endswith("thread_ids.csv"):
                        continue
                    checked += 1
                    rows = data_rows(path)
                    if rows != expected:
                        wrong.append(f"{path.relative_to(suite)}: {rows} rows")
    if wrong:
        shown = "; ".join(wrong[:5]) + (" ..." if len(wrong) > 5 else "")
        raise CheckError(f"{len(wrong)} time series do not have measure_frames={expected} rows: {shown}")
    print(f"Frames: {checked} time series, all with {expected} rows")


def run(suite: Path, only_n: int | None = None) -> None:
    """Checks, then all CSVs of the suite directory; raises CheckError, ValueError, ... on any failure."""
    check_status(suite)
    check_frames(suite, only_n)
    verify_counters_and_evaluate_means.run(suite, None, only_n)
    _, differences = verify_runtime_results.run(suite, only_n)
    if differences != 0:
        raise CheckError(f"runtime results differ between runs/variants ({differences} differences, "
                         "see simulation_runtime_differences.csv)")
    evaluate_system_means.run(suite, only_n)
    evaluate_timing_means.run(suite, only_n)
    evaluate_collision_phases.run(suite, only_n)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--suite", required=True, metavar="NAME", help="Suite directory name below --root")
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT, help="Root directory containing the suites")
    parser.add_argument("--n", type=int, help="Only this n (default: all)")
    args = parser.parse_args()
    try:
        run(resolve_suite(args.root, args.suite), args.n)
    except (CheckError, FileNotFoundError, ValueError) as error:
        print(f"ABORTED: {error}", file=sys.stderr)
        return 1
    print("All CSVs written.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

