#!/usr/bin/env python3
"""Compare runtime-result CSVs row by row and save the number of differences.

For each n, all selected runs and variants are compared with the first
selected run of the first variant (the existing v<k> directories, ascending). CSV row order, index, x, y, z and
their exact string representations must match. No sorting or numerical conversion is performed.
"""

import argparse
import csv
import re
import sys
from itertools import zip_longest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import DEFAULT_ROOT, resolve_suite  # noqa: E402

FILES = (
    "position_go.csv",
    "velocity_go.csv",
    "position_prt.csv",
    "velocity_prt.csv"
)
OUTPUT_NAME = "simulation_runtime_differences.csv"


def count_differences(reference: Path, current: Path) -> int:
    """Count differing CSV fields at the same row and column, including index."""
    with reference.open(newline="", encoding="utf-8-sig") as a, current.open(
            newline="", encoding="utf-8-sig"
    ) as b:
        differences = 0
        for left, right in zip_longest(csv.reader(a), csv.reader(b)):
            if left != right:
                # Extra/missing rows count once per field; even an empty row counts.
                differences += max(
                    1,
                    sum(x != y for x, y in zip_longest(left or [], right or [])),
                )
        return differences


def run(suite: Path, only_n: int | None = None, runs_arg: str | None = None) -> tuple[Path, int]:
    """Compare the runtime results of the suite directory; returns (output CSV, total number of differences)."""
    if not suite.is_dir():
        raise FileNotFoundError(suite)

    n_dirs = (
        [suite / f"n_{only_n}"] if only_n is not None
        else sorted(
            (p for p in suite.glob("n_*") if p.is_dir() and p.name[2:].isdigit()),
            key=lambda p: int(p.name[2:]),
        )
    )
    if not n_dirs:
        raise ValueError(f"No n_ directories in {suite}")

    selected_runs = [int(r) for r in runs_arg.split(",")] if runs_arg else None
    results = []
    for n_dir in n_dirs:
        reference = None
        variant_reference = None
        # variants are derived from the existing v<k> directories
        variants = sorted(int(m.group(1)) for p in n_dir.iterdir() if p.is_dir()
                          if (m := re.fullmatch(r"v(\d+)", p.name)))
        if not variants:
            raise ValueError(f"No variant directories in {n_dir}")
        for variant in variants:
            variant_dir = n_dir / f"v{variant}"
            runs = (
                [variant_dir / f"run_{i:02d}" for i in selected_runs]
                if selected_runs else sorted(
                    (p for p in variant_dir.glob("run_*") if p.is_dir()),
                    key=lambda p: int(p.name.removeprefix("run_")),
                )
            )
            if not runs:
                raise ValueError(f"No runs in {variant_dir}")
            for run_dir in runs:
                if not run_dir.is_dir():
                    raise FileNotFoundError(run_dir)
                if reference is None:
                    variant_reference = variant
                    reference = run_dir  # First selected run of the first variant, separately for each n.
                differences = sum(
                    count_differences(reference / filename, run_dir / filename)
                    for filename in FILES
                )
                # make sure output contains the reference run
                results.append((int(n_dir.name[2:]), f"v{variant_reference}/{reference.name}", f"v{variant}", run_dir.name, differences))

    output = suite / OUTPUT_NAME
    with output.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(("n", "reference", "variant", "run", "different_values"))
        writer.writerows(results)

    total = sum(row[4] for row in results)
    if total == 0:
        print(f"OK, wrote result to {output}")
    else:
        print(f"Differences found: {total} (details: {output})")
    return output, total


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT, help="Root directory containing the suites")
    parser.add_argument("--suite", required=True, metavar="NAME", help="Suite directory name below --root")
    parser.add_argument("--n", type=int, help="Only check one n")
    parser.add_argument("--runs", help="Run IDs, e.g. 1,2,3 (default: all)")
    args = parser.parse_args()
    _, total = run(resolve_suite(args.root, args.suite), args.n, args.runs)
    if total != 0:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
