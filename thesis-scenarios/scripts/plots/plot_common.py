"""Shared helpers of the plot scripts: CSV loading, variant colours and text helpers.

The plot scripts read only the CSVs written by evaluation/evaluate_all.py; they never read the raw run data.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import pandas as pd  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))
import plot_style  # noqa: E402

plot_style.apply_style()

DEFAULT_ROOT = (Path(__file__).resolve().parent / "../../../../../thesis-measurements/suites").resolve()

# Fixed colour per variant (independent of the order of the variants in a suite)
VARIANT_COLORS = {"v1": "#1f77b4", "v2": "#ff7f0e", "v3": "#2ca02c", "v5": "#d62728"}
_FALLBACK_COLORS = ["#9467bd", "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"]


class MissingCsvError(Exception):
    """A CSV written by evaluate_all.py does not exist."""


def color_of(variant: str, variants: list[str] | None = None) -> str:
    """Colour of a variant; V1 #1f77b4, V2 #ff7f0e, V3 #2ca02c, V5 #d62728."""
    if variant in VARIANT_COLORS:
        return VARIANT_COLORS[variant]
    return _FALLBACK_COLORS[sum(map(ord, variant)) % len(_FALLBACK_COLORS)]


def vname(variant: str) -> str:
    """Variant name in the notation of the thesis: "v5" -> $V_{5}$ (other names unchanged)."""
    match = re.fullmatch(r"v(\d+)", str(variant))
    return f"$V_{{{match.group(1)}}}$" if match else str(variant)


def format_us(value: float) -> str:
    """Time in µs with a sensible number of digits (no thousands separator)."""
    if value >= 100:
        return f"{value:.0f}"
    if value >= 10:
        return f"{value:.1f}"
    return f"{value:.2f}"


def wrap_math(text: str, width: int) -> str:
    """
    Line wrapping for captions with mathtext: $...$ expressions are never split (the formula would not be
    rendered) and count for the line length only with about 6 characters (rendered instead of source length).
    """
    protected = re.sub(r"\$[^$]*\$", lambda m: m.group(0).replace(" ", "\x00"), text)
    lines: list[str] = []
    current, length = [], 0
    for word in protected.split():
        visual = len(re.sub(r"\$[^$]*\$", "xxxxxx", word))
        if current and length + 1 + visual > width:
            lines.append(" ".join(current))
            current, length = [], 0
        length += visual + (1 if current else 0)
        current.append(word)
    if current:
        lines.append(" ".join(current))
    return "\n".join(lines).replace("\x00", " ")


def runs_text(runs: list[int]) -> str:
    """'1 Run', '3 Runs' or '2–3 Runs' from the (distinct) run counts."""
    runs = sorted(set(int(r) for r in runs))
    if not runs:
        return "? Runs"
    if len(runs) == 1:
        return f"{runs[0]} Run" if runs[0] == 1 else f"{runs[0]} Runs"
    return f"{runs[0]}–{runs[-1]} Runs"


def resolve_suite(root: Path, name: str) -> Path:
    suite = Path(root) / name
    if not suite.is_dir():
        raise FileNotFoundError(f"Suite directory does not exist: {suite}")
    return suite


def load_csv(suite: Path, name: str) -> pd.DataFrame:
    """Read <suite>/<name>; a missing file points to evaluate_all.py."""
    path = suite / name
    if not path.is_file():
        raise MissingCsvError(
            f"{path} is missing. Create it first: python scripts/evaluation/evaluate_all.py --suite {suite.name}")
    return pd.read_csv(path)

