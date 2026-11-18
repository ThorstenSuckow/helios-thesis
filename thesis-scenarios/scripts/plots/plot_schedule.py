"""02_schedule_ueber_n.png and 02_schedule_ueber_n_gesamt.png: schedule time t_S over n.

Data: timing_means.csv (mean_schedule_us, min_run_mean_schedule_us, max_run_mean_schedule_us, runs).
"""

from __future__ import annotations

import sys
from pathlib import Path

import matplotlib
import matplotlib.lines
import matplotlib.pyplot as plt
import matplotlib.ticker
import matplotlib.transforms
import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
from plot_common import color_of, vname, wrap_math  # noqa: E402
from plot_style import PRINT_WIDTH, save_figure  # noqa: E402

LOAD_RANGES = [   # (title, n_min, n_max)
    ("(a) geringe Last", 144, 2304),
    ("(b) mittlere Last", 4096, 50176),
    ("(c) hohe Last", 65536, 112896),
]
# V3/V5 solid with filled markers; V1/V2 dashed with open markers on top
LOAD_STYLES = {"v3": ("-", True, 3), "v5": ("-", True, 3), "v1": ("--", False, 5), "v2": ("--", False, 5)}


def schedule_series(timing: pd.DataFrame) -> tuple[list[dict], list[int], str, int | None]:
    """Series per variant [µs], the measured n, a note on deviating run counts and the common run count."""
    variants = sorted(timing["variant"].unique())
    ns = sorted(int(n) for n in timing["n"].unique())
    series = []
    for v in variants:
        sub = timing[timing["variant"] == v].sort_values("n")
        series.append({"v": v, "label": vname(v), "color": color_of(v, variants),
                       "x": sub["n"].to_numpy(dtype=float), "y": sub["mean_schedule_us"].to_numpy(),
                       "low": sub["min_run_mean_schedule_us"].to_numpy(),
                       "high": sub["max_run_mean_schedule_us"].to_numpy()})
    counts = {(int(r.n), r.variant): int(r.runs) for r in timing.itertuples()}
    values = sorted(set(counts.values()))
    note = ""
    if len(values) > 1:
        common = max(values, key=list(counts.values()).count)
        deviating = [f"{vname(v)}, $n$ = {n}: {k}" for (n, v), k in sorted(counts.items()) if k != common]
        note = f"Abweichende Anzahl Runs (sonst {common}): " + "; ".join(deviating) + "."
    return series, ns, note, (values[0] if len(values) == 1 else None)


def _overlapping(artists, renderer, pad: float = 1.5) -> list[tuple]:
    """Pairs of overlapping texts (window coordinates, with a small margin)."""
    boxes = []
    for a in artists:
        b = a.get_window_extent(renderer)
        boxes.append((a, matplotlib.transforms.Bbox.from_extents(b.x0 - pad, b.y0 - pad, b.x1 + pad, b.y1 + pad)))
    return [(boxes[i][0], boxes[j][0]) for i in range(len(boxes)) for j in range(i + 1, len(boxes))
            if boxes[i][1].overlaps(boxes[j][1])]


def label_n_axis(fig, ax, ns: list[int], pbar: dict[int, float], thin: bool = True, fontsize: float = 8):
    """
    Logarithmic x axis, ticks at the measured n. Vertical labels: n and, below it (teal), the mean particle
    population. Overlapping labels are omitted (the tick stays). Returns (omitted n, label depth below the axis in
    inches).
    """
    ax.set_xscale("log")
    ax.set_xlim(min(ns) / 1.15, max(ns) * 1.15)
    ax.set_xticks(list(ns))
    ax.set_xticklabels([])
    ax.xaxis.set_minor_locator(matplotlib.ticker.NullLocator())
    ax.tick_params(axis="x", length=3)
    items = []
    for n in ns:
        t = ax.annotate(str(n), xy=(n, 0), xycoords=ax.get_xaxis_transform(), xytext=(0, -4), textcoords="offset points",
                        rotation=90, ha="center", va="top", fontsize=fontsize, annotation_clip=False)
        p = pbar.get(n)
        a = None
        if p is not None:
            a = ax.annotate(rf"$\overline{{P^p}}$ {p:.0f}", xy=(0.5, 0), xycoords=t, xytext=(0, -2),
                            textcoords="offset points", rotation=90, ha="center", va="top", fontsize=8, color="teal",
                            annotation_clip=False)
        items.append((n, t, a))
    renderer = fig.canvas.get_renderer()

    def box(item):
        _, t, a = item
        bt = t.get_window_extent(renderer)
        bb = matplotlib.transforms.Bbox.union([bt] + ([a.get_window_extent(renderer)] if a is not None else []))
        return matplotlib.transforms.Bbox.from_extents(bb.x0 - 2, bb.y0, bb.x1 + 2, bb.y1)

    kept = [items[0]]
    for item in (items[1:-1] if thin else []):
        if not box(item).overlaps(box(kept[-1])):
            kept.append(item)
    if not thin:
        kept = list(items)
    elif len(items) > 1:
        while len(kept) > 1 and box(items[-1]).overlaps(box(kept[-1])):
            kept.pop()
        kept.append(items[-1])
    kept_n = {item[0] for item in kept}
    omitted = []
    for item in items:
        if item[0] not in kept_n:
            omitted.append(item[0])
            item[1].remove()
            if item[2] is not None:
                item[2].remove()
    visible = [x for item in kept for x in (item[1], item[2]) if x is not None]
    clashes = _overlapping(visible, renderer, pad=0.0)
    if clashes:
        print(f"WARNUNG: {len(clashes)} überlappende Tick-Beschriftungen", file=sys.stderr)
    lowest = min(x.get_window_extent(renderer).y0 for x in visible)
    depth_in = (ax.bbox.y0 - lowest) / fig.dpi
    return omitted, depth_in


def scaling_figure(path: Path, series: list[dict], ns: list[int], *, title: str, ylabel: str, caption: str,
                   note: str = "") -> None:
    """Lines over n (mean of the run means, band: minimum to maximum), logarithmic x axis, linear y axis from 0."""
    width = PRINT_WIDTH
    plot_h, left_in, right_in, title_h = 2.5, 0.7, 0.15, 1.3
    fig = plt.figure(figsize=(width, 6.0))
    ax = fig.add_axes([0.1, 0.1, 0.8, 0.8])

    def layout(cap_h: float, label_h: float) -> float:
        height = cap_h + label_h + plot_h + title_h
        fig.set_size_inches(width, height)
        bottom = cap_h + label_h
        ax.set_position([left_in / width, bottom / height, (width - left_in - right_in) / width, plot_h / height])
        return height

    layout(1.0, 1.5)
    for s in series:
        ax.plot(s["x"], s["y"], marker="o", color=s["color"], label=s["label"], linestyle="-")
        ax.fill_between(s["x"], s["low"], s["high"], color=s["color"], alpha=0.2, linewidth=0)
    ax.set_ylabel(ylabel)
    ax.grid(alpha=0.3)
    top = max(float(np.max(s["high"])) for s in series)
    ax.set_ylim(0, top * 1.08)
    ax.set_title(wrap_math(title, 78), loc="left", pad=22)
    handles, _ = ax.get_legend_handles_labels()
    ax.legend(loc="lower right", bbox_to_anchor=(1.0, 1.0), ncols=min(len(handles), 6), frameon=False, fontsize=8)
    omitted, depth_in = label_n_axis(fig, ax, ns, {}, thin=False, fontsize=7)

    ax.text(0.5, -(depth_in + 0.1) / plot_h, "$n$ (Anzahl GameObjects), logarithmische Skala",
            transform=ax.transAxes, ha="center", va="top", fontsize=9)
    extras = caption
    if note:
        extras += " " + note
    wrapped = wrap_math(extras, 100)
    cap_h = (0.15 * (wrapped.count("\n") + 1) + 0.25) if wrapped.strip() else 0.1
    height = layout(cap_h, depth_in + 0.55)
    fig.text(0.1 / width, 0.05 / height, wrapped, ha="left", va="bottom", fontsize=7, color="0.2")
    save_figure(fig, path)
    plt.close(fig)


def schedule_load_ranges_figure(path: Path, series: list[dict], runs: int | None) -> None:
    """02_schedule_ueber_n: t_S over n in three detail views (load ranges), each with its own linear y scale."""
    height = 7.9
    fig, axes = plt.subplots(3, 1, figsize=(PRINT_WIDTH, height))
    for ax, (title, n_lo, n_hi) in zip(axes, LOAD_RANGES):
        lows, highs, xs_all = [], [], set()
        for s in sorted(series, key=lambda s: LOAD_STYLES.get(s["v"], ("-", True, 3))[2]):
            style, filled, z = LOAD_STYLES.get(s["v"], ("-", True, 3))
            sel = (s["x"] >= n_lo) & (s["x"] <= n_hi)
            if not sel.any():
                continue
            x, y, lo, hi = s["x"][sel], s["y"][sel], s["low"][sel], s["high"][sel]
            ax.fill_between(x, lo, hi, color=s["color"], alpha=0.18, linewidth=0, zorder=z - 1)
            ax.plot(x, y, linestyle=style, color=s["color"], linewidth=1.3, marker="o", markersize=4.5,
                    markerfacecolor=s["color"] if filled else "white", markeredgecolor=s["color"], markeredgewidth=1.0,
                    zorder=z)
            lows += list(lo)
            highs += list(hi)
            xs_all |= {int(v) for v in x}
        if not xs_all:
            ax.set_visible(False)
            continue
        ticks = sorted(xs_all)
        span = max(ticks) - min(ticks)
        pad = 0.04 * span if span else 1.0
        ax.set_xlim(min(ticks) - pad, max(ticks) + pad)
        ax.set_xticks(ticks)
        # horizontal labels only if the smallest tick gap fits 5 digits (about 0.45 in)
        axis_in = PRINT_WIDTH * 0.85
        gap_in = min(np.diff(ticks)) / (span + 2 * pad) * axis_in if len(ticks) > 1 else 9.0
        ax.set_xticklabels([str(t) for t in ticks], rotation=0 if gap_in >= 0.5 else 90)
        top, bottom = max(highs), min(lows)
        margin = 0.08 * (top - bottom) if top > bottom else 0.05 * top
        ax.set_ylim(0 if title.startswith("(a)") else bottom - margin, top + margin)
        ax.set_xlabel("$n$")
        ax.set_ylabel("Ausführungszeit [µs]")
        ax.set_title(title, loc="left", fontsize=9)
        ax.grid(alpha=0.3)
    handles = [matplotlib.lines.Line2D([], [], color=s["color"], linestyle=LOAD_STYLES.get(s["v"], ("-", True, 3))[0],
                                       marker="o", markersize=4.5, markeredgewidth=1.0,
                                       markerfacecolor=s["color"] if LOAD_STYLES.get(s["v"], ("-", True, 3))[1] else "white",
                                       label=s["label"]) for s in series]
    fig.legend(handles=handles, loc="upper center", ncols=len(handles), frameon=False, fontsize=9, bbox_to_anchor=(0.5, 0.995))
    runs_txt = f" ({runs} Runs)" if runs else ""
    caption = wrap_math(f"Linie: Mittelwert der Run-Mittelwerte; Band: Minimum bis Maximum der Run-Mittelwerte{runs_txt}.", 100)
    lines = caption.count("\n") + 1
    bottom_in = 0.15 + 0.16 * lines
    fig.text(0.02, 0.08 / height, caption, ha="left", va="bottom", fontsize=8, color="0.2")
    fig.tight_layout(rect=(0, bottom_in / height, 1, 1 - 0.4 / height))
    save_figure(fig, path)
    plt.close(fig)


def plot_schedule(timing: pd.DataFrame, out_dir: Path) -> list[Path]:
    """Create 02_schedule_ueber_n_gesamt.png and 02_schedule_ueber_n.png; returns the written paths."""
    out_dir.mkdir(parents=True, exist_ok=True)
    series, ns, runs_note, runs = schedule_series(timing)
    t_sym = r"$\overline{t_{\mathcal{S}}}$"
    scaling_figure(out_dir / "02_schedule_ueber_n_gesamt.png", series, ns,
                   title=f"Schedule-Laufzeit {t_sym} über $n$", ylabel="Ausführungszeit [µs]",
                   caption="Linie: Mittelwert der Run-Mittelwerte; Band: Minimum bis Maximum der Run-Mittelwerte. "
                           "Logarithmische x-Achse, lineare y-Achse.", note=runs_note)
    schedule_load_ranges_figure(out_dir / "02_schedule_ueber_n.png", series, runs)
    return [out_dir / "02_schedule_ueber_n_gesamt.png", out_dir / "02_schedule_ueber_n.png"]

