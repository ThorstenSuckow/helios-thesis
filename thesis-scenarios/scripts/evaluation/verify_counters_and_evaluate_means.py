#!/usr/bin/env python3
"""Verify per-frame ECS counters and calculate their means.

Each CSV contains frame,<counter>. Rows (including frame N) must match
exactly, in their original order, across all selected runs and variants.
No sorting or frame-index alignment is performed.

Expected layout:
    <root>/<suite>/n_<n>/v<k>/run_<id>/*.csv
The variants are the existing v<k> directories; all of them must have identical counters.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import DEFAULT_ROOT, resolve_suite  # noqa: E402

OUTPUT_NAME = "simulation_counter_means.csv"
N_DIR_PATTERN = re.compile(r"n_(\d+)$")
RUN_DIR_PATTERN = re.compile(r"run_(\d+)$")

METRIC_ALIASES: dict[str, tuple[str, ...]] = {
    "collision_commands_go": ("collision_commands_go",),
    "collision_pairs_found_go": ("collision_pairs_found_go",),
    "collision_pairs_found_prt": ("collision_pairs_found_prt",),
    "collision_pairs_processed_go": ("collision_pairs_processed_go",),
    "collision_pairs_processed_prt": ("collision_pairs_processed_prt",),
    "lifetime_commands_prt": ("lifetime_commands_prt",),
    "particle_population": ("particle_population",),
}


def normalize_name(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.lower())


def counter_files(run_dir: Path) -> dict[str, Path]:
    candidates: dict[str, list[Path]] = {}
    for path in run_dir.glob("*.csv"):
        candidates.setdefault(normalize_name(path.stem), []).append(path)

    result = {}
    for metric, aliases in METRIC_ALIASES.items():
        found = [path for alias in aliases for path in candidates.get(normalize_name(alias), [])]
        if len(found) != 1:
            if not found:
                raise FileNotFoundError(f"Missing '{metric}' in {run_dir}")
            raise ValueError(f"Multiple files for '{metric}' in {run_dir}: {found}")
        result[metric] = found[0]
    return result


def read_counter(path: Path, metric: str) -> pd.DataFrame:
    data = pd.read_csv(path, encoding="utf-8-sig")
    if data.shape[1] != 2 or str(data.columns[0]).strip().lower() != "frame":
        raise ValueError(f"Invalid header in {path}: expected frame,<counter>")
    if data.empty or data.isna().any().any():
        raise ValueError(f"Empty or incomplete CSV: {path}")
    if not all(pd.api.types.is_integer_dtype(dtype) for dtype in data.dtypes):
        raise ValueError(f"Frame and counter must be integers: {path}")
    if data.iloc[:, 0].duplicated().any():
        raise ValueError(f"Duplicate frame in {path}")
    if (data.iloc[:, 1] < 0).any():
        raise ValueError(f"Negative counter in {path}")
    data.columns = ["frame", metric]
    return data  # Preserve CSV row order. Do not sort or set_index('frame').


def assert_identical(reference: pd.DataFrame, current: pd.DataFrame, label: str) -> None:
    """Require the same number of rows and identical values in every CSV row."""
    if reference.equals(current):
        return
    if len(reference) != len(current):
        raise ValueError(f"{label}: different row counts ({len(reference)} vs {len(current)})")
    if list(reference.columns) != list(current.columns):
        raise ValueError(f"{label}: different columns")

    different = reference.ne(current)
    row, column = different.stack().loc[lambda s: s].index[0]
    raise ValueError(
        f"{label}: CSV row {row + 2}, field '{column}': "
        f"reference={reference.at[row, column]}, current={current.at[row, column]}"
    )


def read_run(run_dir: Path) -> pd.DataFrame:
    files = counter_files(run_dir)
    counters = {metric: read_counter(path, metric) for metric, path in files.items()}

    first = next(iter(counters.values()))
    result = first.copy()
    for metric, data in list(counters.items())[1:]:
        assert_identical(
            result[["frame"]], data[["frame"]],
            f"Different frame sequence within {run_dir}: {metric}",
        )
        result[metric] = data[metric]
    return result


def run_directories(variant_dir: Path, selected_runs: set[int] | None) -> list[Path]:
    if not variant_dir.is_dir():
        raise FileNotFoundError(f"Variant directory does not exist: {variant_dir}")
    candidates = {
        int(match.group(1)): path
        for path in variant_dir.iterdir() if path.is_dir()
        if (match := RUN_DIR_PATTERN.fullmatch(path.name))
    }
    if selected_runs is not None:
        missing = sorted(selected_runs - candidates.keys())
        if missing:
            raise ValueError(f"Missing selected runs in {variant_dir}: {missing}")
        candidates = {run_id: candidates[run_id] for run_id in selected_runs}
    if not candidates:
        raise ValueError(f"No runs found in {variant_dir}")
    return [candidates[run_id] for run_id in sorted(candidates)]


def summarize_variant(variant_dir: Path, selected_runs: set[int] | None) -> tuple[int, pd.DataFrame]:
    runs = run_directories(variant_dir, selected_runs)
    reference = read_run(runs[0])
    for run in runs[1:]:
        assert_identical(reference, read_run(run), f"{runs[0]} vs {run}")
    return len(runs), reference


def variant_ids(n_dir: Path) -> list[int]:
    """Variant numbers k of the existing v<k> directories of an n directory, ascending."""
    return sorted(int(match.group(1)) for path in n_dir.iterdir() if path.is_dir()
                  if (match := re.fullmatch(r"v(\d+)", path.name)))


def parse_run_ids(value: str) -> set[int]:
    try:
        runs = {int(part.strip()) for part in value.split(",")}
    except ValueError as exc:
        raise argparse.ArgumentTypeError("Expected comma-separated run IDs, e.g. 1,3,5") from exc
    if not runs or any(run < 1 for run in runs):
        raise argparse.ArgumentTypeError("Run IDs must be positive integers")
    return runs


def run(suite: Path, selected_runs: set[int] | None = None, only_n: int | None = None,
        output: Path | None = None) -> Path:
    """Verify the counters of the suite directory and write simulation_counter_means.csv."""
    n_dirs = sorted(
        (
            (int(match.group(1)), path)
            for path in suite.iterdir() if path.is_dir()
            if (match := N_DIR_PATTERN.fullmatch(path.name))
        ),
        key=lambda item: item[0],
    )
    if only_n is not None:
        n_dirs = [(n, path) for n, path in n_dirs if n == only_n]
    if not n_dirs:
        raise ValueError(f"No matching n_ directories under {suite}")

    rows = []
    for n, n_dir in n_dirs:
        variant_numbers = variant_ids(n_dir)   # derived from the existing v<k> directories
        if not variant_numbers:
            raise ValueError(f"No variant directories in {n_dir}")
        variants = {
            variant: summarize_variant(n_dir / f"v{variant}", selected_runs)
            for variant in variant_numbers
        }
        first = variant_numbers[0]
        reference = variants[first][1]
        for variant in variant_numbers[1:]:
            assert_identical(reference, variants[variant][1], f"n={n}: v{first} vs v{variant}")

        for variant, (run_count, data) in variants.items():
            row = {"n": n, "variant": f"v{variant}", "runs": run_count, "frames": len(data)}
            row.update({f"mean_{metric}": data[metric].mean() for metric in METRIC_ALIASES})
            rows.append(row)

    output = output if output is not None else suite / OUTPUT_NAME
    output.parent.mkdir(parents=True, exist_ok=True)
    fields = ["n", "variant", "runs", "frames", *(f"mean_{m}" for m in METRIC_ALIASES)]
    pd.DataFrame(rows, columns=fields).to_csv(output, index=False, float_format="%.6f")
    print(f"OK — saved {len(rows)} rows to {output}")
    return output


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", required=True, metavar="NAME", help="Suite directory name below --root")
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT, help="Root directory containing the suites")
    parser.add_argument("--runs", type=parse_run_ids, help="Comma-separated IDs (default: all)")
    parser.add_argument("--n", type=int, help="Only this n (default: all)")
    parser.add_argument("--output", type=Path, help=f"Output CSV (default: suite/{OUTPUT_NAME})")
    args = parser.parse_args()
    run(resolve_suite(args.root, args.suite), args.runs, args.n, args.output)


if __name__ == "__main__":
    main()
