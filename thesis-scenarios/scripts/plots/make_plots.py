#!/usr/bin/env python3
"""Create the figures of the thesis from the evaluation CSVs of a suite.

Usage:
    python scripts/plots/make_plots.py --suite <name> [--root <dir>] [--out <dir>] [--n 1024 16384 112896]

Reads only (written by scripts/evaluation/evaluate_all.py; no access to run_*/):
    <suite>/timing_means.csv, system_means.csv, collision_phase_means.csv, simulation_counter_means.csv

Output (default <suite>/plots/, same folder structure as content/080-evaluierung/img/):
    laufzeit/02_schedule_ueber_n.png
    laufzeit/02_schedule_ueber_n_gesamt.png
    laufzeit/10a_gemeinsame_systeme.png      update(), without flush(); one row per n
    laufzeit/10b_kollisionsverarbeitung.png  collision phases, with flush(); one row per n
    laufzeit/10c_kollisionssysteme.png       update(), without flush(); one row per n
    zaehlwerte/13_entitaetspopulation_ueber_n.png
    preview.pdf    10a to 10c, one page per figure at the size in the thesis (text area)
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import plot_style  # noqa: E402
from plot_common import DEFAULT_ROOT, MissingCsvError, load_csv, resolve_suite  # noqa: E402
from plot_counts import plot_population  # noqa: E402
from plot_schedule import plot_schedule  # noqa: E402
from plot_systems import plot_systems  # noqa: E402

DEFAULT_NS = (1024, 16384, 112896)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--suite", required=True, metavar="NAME", help="Suite directory name below --root")
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT, help="Root directory containing the suites")
    parser.add_argument("--out", type=Path, help="Output directory (default: <suite>/plots)")
    parser.add_argument("--n", type=int, nargs="+", default=list(DEFAULT_NS),
                        help="n rows of the figures 10a to 10c, top to bottom (default: 1024 16384 112896)")
    args = parser.parse_args()
    try:
        suite = resolve_suite(args.root, args.suite)
        timing = load_csv(suite, "timing_means.csv")
        systems = load_csv(suite, "system_means.csv")
        phases = load_csv(suite, "collision_phase_means.csv")
        counters = load_csv(suite, "simulation_counter_means.csv")
    except (MissingCsvError, FileNotFoundError) as error:
        print(f"ABORTED: {error}", file=sys.stderr)
        return 1

    out = args.out or suite / "plots"
    plot_schedule(timing, out / "laufzeit")
    paths, _ = plot_systems(systems, phases, list(args.n), out / "laufzeit")
    plot_population(counters, out / "zaehlwerte")
    plot_style.print_report()
    print(f"Figures written to {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())






