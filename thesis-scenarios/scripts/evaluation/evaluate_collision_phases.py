#!/usr/bin/env python3
"""Phase times of the collision systems (det = detection, res = resolution) per n and variant.

Per frame and run:
    V1, V2: time of s_delta_<phase>_go (covers both tags in these variants)
    V3:     s_delta_<phase>_go + s_delta_<phase>_prt of the same frame
    V5:     duration of the parallel system group p_<phase> (not the sum of the single systems)
update = without flush(), total = update + flush() (<system>_flush_timings.csv; a missing file means 0).
p_det/p_res have no separate flush series: the group duration is taken to include distribution, synchronisation
and flush(), so for V5 only total_* is filled and update_* stays empty.

Aggregation: per run the mean over all frames -> run means per n, variant and phase; then mean, min and max
over the runs. The V3 band is formed from the run means of the sum.

Output (<suite>/):
    collision_phase_means.csv      n,variant,phase,runs,update_*/total_* mean, min and max of the run means [us]
Checks (printed): V1/V2/V3 against system_means.csv (needs it; run evaluate_system_means.py first),
V5: p_<phase> >= max(s_delta_<phase>_go, s_delta_<phase>_prt) per frame, number of violations.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import DEFAULT_ROOT, discover_layout, resolve_suite  # noqa: E402

PHASES = ("det", "res")
# V3 and its implementation variant V6: sum of the go and prt systems per frame
SUM_VARIANTS = ("v3", "v6")
TOLERANCE_US = 0.01


def series(run_dir: Path, name: str) -> pd.Series | None:
    """Time series [ns] indexed by frame; None if the file does not exist."""
    path = run_dir / f"{name}.csv"
    if not path.exists():
        return None
    df = pd.read_csv(path)
    return df.set_index(df.columns[0])[df.columns[1]].astype(float)


def system_time(run_dir: Path, system: str) -> tuple[pd.Series, pd.Series]:
    """(update, total) of a system per frame."""
    update = series(run_dir, f"{system}_timings")
    if update is None:
        raise FileNotFoundError(run_dir / f"{system}_timings.csv")
    flush = series(run_dir, f"{system}_flush_timings")
    flush = update * 0.0 if flush is None else flush.reindex(update.index)
    if flush.isna().any():
        raise ValueError(f"{run_dir}: frames of {system} flush and update differ")
    return update, update + flush


def phase_series(run_dir: Path, variant: str, phase: str) -> tuple[pd.Series | None, pd.Series]:
    """(update or None, total) of the phase per frame [ns]."""
    if variant in ("v1", "v2"):
        return system_time(run_dir, f"s_delta_{phase}_go")
    if variant in SUM_VARIANTS:
        go_u, go_t = system_time(run_dir, f"s_delta_{phase}_go")
        prt_u, prt_t = system_time(run_dir, f"s_delta_{phase}_prt")
        if not go_u.index.equals(prt_u.index):
            raise ValueError(f"{run_dir}: frames of go and prt differ ({phase})")
        return go_u + prt_u, go_t + prt_t
    if variant == "v5":
        group = series(run_dir, f"p_{phase}_timings")
        if group is None:
            raise FileNotFoundError(run_dir / f"p_{phase}_timings.csv")
        return None, group
    raise ValueError(f"unknown variant {variant}")


def run(suite: Path, only_n: int | None = None) -> Path:
    """Write collision_phase_means.csv for the suite directory; returns the output path."""
    layout = discover_layout(suite, only_n)

    rows = []
    violations = {phase: 0 for phase in PHASES}
    frames_checked = {phase: 0 for phase in PHASES}
    for n, variants in sorted(layout.items()):
        for variant, runs in variants.items():
            for phase in PHASES:
                upd, tot = [], []
                for run_dir in runs:
                    u, t = phase_series(run_dir, variant, phase)
                    upd.append(None if u is None else u.mean())
                    tot.append(t.mean())
                    if variant == "v5":
                        g_u, _ = system_time(run_dir, f"s_delta_{phase}_go")
                        p_u, _ = system_time(run_dir, f"s_delta_{phase}_prt")
                        longest = pd.concat([g_u, p_u], axis=1).max(axis=1)
                        # p_<phase> contains flush(), the single systems are compared as update (as logged)
                        frames_checked[phase] += len(t)
                        violations[phase] += int((t.reindex(longest.index) < longest).sum())
                row = {"n": n, "variant": variant, "phase": phase, "runs": len(runs)}
                for label, values in (("update", upd), ("total", tot)):
                    s = pd.Series(values, dtype=float) / 1000.0
                    row[f"{label}_mean_us"] = s.mean() if s.notna().any() else None
                    row[f"{label}_min_run_mean_us"] = s.min() if s.notna().any() else None
                    row[f"{label}_max_run_mean_us"] = s.max() if s.notna().any() else None
                rows.append(row)

    out = pd.DataFrame(rows)
    out.to_csv(suite / "collision_phase_means.csv", index=False, float_format="%.6f")
    print(f"OK - {len(out)} rows -> {suite / 'collision_phase_means.csv'}")
    print("V5: p_det/p_res have no separate flush series; assumed to include flush(); update_* left empty.")

    # Checks against system_means.csv
    sys_path = suite / "system_means.csv"
    if sys_path.exists():
        sysm = pd.read_csv(sys_path).set_index(["n", "variant", "system"])
        for variant in ("v1", "v2", *SUM_VARIANTS):
            for col in ("update_mean_us", "total_mean_us"):
                worst, count = 0.0, 0
                for phase in PHASES:
                    sub = out[(out.variant == variant) & (out.phase == phase)]
                    for _, r in sub.iterrows():
                        parts = [f"s_delta_{phase}_go"] + ([f"s_delta_{phase}_prt"] if variant in SUM_VARIANTS else [])
                        ref = sum(sysm.loc[(r.n, variant, p), col] for p in parts)
                        worst = max(worst, abs(r[col] - ref))
                        count += 1
                status = "OK" if worst < TOLERANCE_US else "ABWEICHUNG"
                print(f"Kontrolle {variant} {col}: {count} Werte, max. Abweichung {worst:.6f} us -> {status}")
    else:
        print("Kontrolle V1-V3 uebersprungen: system_means.csv fehlt (evaluate_system_means.py ausfuehren).")
    for phase in PHASES:
        print(f"Kontrolle V5 p_{phase} >= max(go, prt): {violations[phase]} Verletzungen "
              f"in {frames_checked[phase]} Frames")
    return suite / "collision_phase_means.csv"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--suite", required=True, metavar="NAME", help="Suite directory name below --root")
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT, help="Root directory containing the suites")
    parser.add_argument("--n", type=int, help="Only this n")
    args = parser.parse_args()
    run(resolve_suite(args.root, args.suite), args.n)


if __name__ == "__main__":
    main()
