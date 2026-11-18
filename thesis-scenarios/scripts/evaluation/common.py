"""Shared helpers of the evaluation scripts: suite lookup and directory layout discovery.

Layout (relative to the scripts directory):
    ../../../../thesis-measurements/suites/<suite>/n_<n>/v<k>/run_<id>/*.csv
"""

from __future__ import annotations

import re
from pathlib import Path

DEFAULT_ROOT = (Path(__file__).resolve().parent / "../../../../../thesis-measurements/suites").resolve()
N_DIR_PATTERN = re.compile(r"n_(\d+)")


def resolve_suite(root: Path, name: str) -> Path:
    """Directory of the suite <name> below <root>; the suite name is never guessed."""
    suite = Path(root) / name
    if not suite.is_dir():
        raise FileNotFoundError(f"Suite directory does not exist: {suite}")
    return suite


def discover_layout(suite: Path, only_n: int | None = None) -> dict[int, dict[str, list[Path]]]:
    """n -> variant directory name -> sorted run directories, derived from the existing directories."""
    layout: dict[int, dict[str, list[Path]]] = {}
    for n_dir in suite.glob("n_*"):
        match = N_DIR_PATTERN.fullmatch(n_dir.name)
        if not match or (only_n is not None and int(match.group(1)) != only_n):
            continue
        layout[int(match.group(1))] = {
            v.name: sorted(r for r in v.glob("run_*") if r.is_dir())
            for v in sorted(n_dir.glob("v*")) if v.is_dir()
        }
    if not layout:
        raise ValueError(f"No n_ directories in {suite}")
    return layout


def check_layout(layout: dict[int, dict[str, list[Path]]]) -> None:
    """Same variants and the same number of runs for every n and variant."""
    reference_n = min(layout)
    reference = {v: len(runs) for v, runs in layout[reference_n].items()}
    for n, variants in sorted(layout.items()):
        found = {v: len(runs) for v, runs in variants.items()}
        if found != reference:
            raise ValueError(f"n={n}: variants/runs {found} differ from n={reference_n} ({reference})")
    if len(set(reference.values())) != 1:
        raise ValueError(f"Number of runs differs between variants: {reference}")

