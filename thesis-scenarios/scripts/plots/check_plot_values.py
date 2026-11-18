#!/usr/bin/env python3
"""Check that the figures 10a, 10b and 10c draw exactly the values of the CSVs.

Usage:
    python scripts/plots/check_plot_values.py --suite <name> [--root <dir>] [--n 1024 16384 112896]

For every bar the drawn mean, minimum and maximum are compared with the CSV (independently read):
    10a, 10c  system_means.csv,          update_mean_us / update_min_run_mean_us / update_max_run_mean_us
    10b       collision_phase_means.csv, total_mean_us  / total_min_run_mean_us  / total_max_run_mean_us
and the value text with the rounded mean. Exit code 0 if everything matches, otherwise 1.
"""

from __future__ import annotations

import argparse
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import plot_style  # noqa: E402
from plot_common import DEFAULT_ROOT, MissingCsvError, format_us, load_csv, resolve_suite  # noqa: E402
from plot_systems import plot_systems  # noqa: E402

DEFAULT_NS = (1024, 16384, 112896)
# 10c, n = 112896, Behandlung (main dataset of the thesis): (item, variant) -> (value in µs, tolerance)
CONTROL_VALUES = {("Go", "v3"): (146.0, 0.05), ("Go", "v5"): (164.4, 0.05),
                  ("Prt", "v3"): (2939.0, 0.5), ("Prt", "v5"): (2829.0, 0.5)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--suite", required=True, metavar="NAME")
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--n", type=int, nargs="+", default=list(DEFAULT_NS))
    parser.add_argument("--control", action="store_true",
                        help="also check the control values of 10c (n = 112896, Behandlung) of the main dataset")
    args = parser.parse_args()
    try:
        suite = resolve_suite(args.root, args.suite)
        systems = load_csv(suite, "system_means.csv")
        phases = load_csv(suite, "collision_phase_means.csv")
    except (MissingCsvError, FileNotFoundError) as error:
        print(f"ABORTED: {error}", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        _, bars = plot_systems(systems, phases, list(args.n), Path(tmp))

    errors: list[str] = []
    shown = 0
    for bar in bars:
        if bar["plot"].startswith("10b"):
            frame, prefix, column = phases, "phase", "total"
        else:
            frame, prefix, column = systems, "system", "update"
        row = frame[(frame["n"] == bar["n"]) & (frame["variant"] == bar["variant"]) & (frame[prefix] == bar["key"])]
        where = f"{bar['plot']} n={bar['n']} {bar['panel']}/{bar['item']} {bar['variant']}"
        if len(row) != 1:
            errors.append(f"{where}: {len(row)} CSV rows for key {bar['key']}")
            continue
        row = row.iloc[0]
        expected = (float(row[f"{column}_mean_us"]), float(row[f"{column}_min_run_mean_us"]),
                    float(row[f"{column}_max_run_mean_us"]))
        if (bar["mean"], bar["min"], bar["max"]) != expected:
            errors.append(f"{where}: drawn {(bar['mean'], bar['min'], bar['max'])} != CSV {expected}")
        if bar["text"] is not None:
            shown += 1
            if bar["text"] != format_us(expected[0]):
                errors.append(f"{where}: text '{bar['text']}' != '{format_us(expected[0])}'")
    dropped = len(bars) - shown

    if args.control:
        for (item, variant), (value, tol) in CONTROL_VALUES.items():
            hit = [b for b in bars if b["plot"].startswith("10c") and b["n"] == 112896 and b["panel"] == "Behandlung"
                   and b["item"] == item and b["variant"] == variant]
            if len(hit) != 1 or abs(hit[0]["mean"] - value) > tol:
                errors.append(f"control {item} {variant}: {hit[0]['mean'] if hit else 'missing'} != {value}")
            else:
                print(f"control 10c n=112896 Behandlung {item} {variant}: {hit[0]['mean']:.3f} µs "
                      f"(text '{hit[0]['text']}') ok")

    print(f"{len(bars)} bars checked, {shown} value texts compared, {dropped} value texts dropped")
    plot_style.print_report()
    for error in errors:
        print("MISMATCH:", error)
    print("OK" if not errors else f"FAILED ({len(errors)} mismatches)")
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())

