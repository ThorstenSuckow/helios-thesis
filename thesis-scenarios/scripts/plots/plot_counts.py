"""13_entitaetspopulation_ueber_n.png: mean particle population per n.

Data: simulation_counter_means.csv (mean_particle_population). verify_counters_and_evaluate_means.py enforces
frame-wise identical counters across all runs and variants, so minimum = maximum = mean of the run means; the band
is drawn with width 0.
"""

from __future__ import annotations

import sys
from pathlib import Path

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.ticker
import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
from plot_common import wrap_math  # noqa: E402
from plot_style import print_size, save_figure  # noqa: E402


def thin_ticks(ns: list[int], axis_in: float, gap_in: float = 0.17) -> list[int]:
    """n whose tick label (rotated by 90 degrees) fits on the logarithmic x axis without overlap."""
    if len(ns) < 2:
        return list(ns)
    span = np.log(max(ns) / min(ns)) + 2 * np.log(1.15)
    per_in = span / axis_in
    kept = [ns[0]]
    for n in ns[1:-1]:
        if np.log(n / kept[-1]) >= gap_in * per_in and np.log(ns[-1] / n) >= gap_in * per_in:
            kept.append(n)
    kept.append(ns[-1])
    return kept


def style_n_axis(ax, ylabel: str, title: str = "", ns: list[int] | None = None, logy: bool = False) -> None:
    """Axis style for plots over n; the x axis is logarithmic, the ticks are at the measured n."""
    ax.set_xscale("log")
    ax.set_xlabel(r"$n$ (logarithmische Skala)")
    ax.set_ylabel(ylabel)
    if title:
        ax.set_title(title, loc="left")
    if logy:
        ax.set_yscale("log")  # y grows roughly proportional to n
        ax.set_ylabel(ylabel + " (log.)")
    else:
        ax.set_ylim(bottom=0)
    ax.grid(alpha=0.3, which="major")
    if ns:
        ax.set_xticks(list(ns))
    shown = set(thin_ticks(ns or [], 5.2))
    ax.xaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(lambda v, _: f"{int(v)}" if int(round(v)) in shown or not ns else ""))
    ax.xaxis.set_minor_locator(matplotlib.ticker.NullLocator())
    ax.tick_params(axis="x", labelrotation=90 if ns and len(ns) > 8 else 0)


def finish(fig, path: Path, caption: str = "") -> None:
    wrapped = "\n".join(wrap_math(part, 90) for part in caption.split("\n")) if caption else ""
    height = fig.get_figheight()
    lines = wrapped.count("\n") + 1 if wrapped else 0
    bottom = (0.12 + 0.15 * lines) / height if lines else 0.0
    fig.tight_layout(rect=(0, bottom, 1, 1))
    if wrapped:
        fig.text(0.02, 0.08 / height, wrapped, ha="left", va="bottom", fontsize=8, color="0.2")
    save_figure(fig, path)
    plt.close(fig)


def plot_population(counters: pd.DataFrame, out_dir: Path) -> Path:
    """13: mean particle population per n (over all variants) and n + particles."""
    out_dir.mkdir(parents=True, exist_ok=True)
    ns = sorted(int(n) for n in counters["n"].unique())
    mean, low, high = [], [], []
    for n in ns:
        values = counters.loc[counters["n"] == n, "mean_particle_population"].to_numpy(dtype=float)
        mean.append(values.mean())
        low.append(values.min())
        high.append(values.max())
    x, mean, low, high = (np.array(a, dtype=float) for a in ([float(n) for n in ns], mean, low, high))
    fig, ax = plt.subplots(figsize=print_size())
    ax.plot(x, mean, marker="o", color="teal", label=r"$\overline{P^p}$ (Partikel)")
    ax.fill_between(x, low, high, color="teal", alpha=0.2, linewidth=0)
    ax.plot(x, x, marker="o", color="tab:orange", label=r"$n$ (GameObjects)")
    ax.plot(x, x + mean, marker="o", color="0.3", linestyle="--",
            label=r"$n + \overline{P^p}$ (GameObjects und Partikel)")
    style_n_axis(ax, "Anzahl Entitäten", r"Entitätspopulation über $n$", ns, logy=True)
    ax.legend()
    path = out_dir / "13_entitaetspopulation_ueber_n.png"
    finish(fig, path)
    return path

