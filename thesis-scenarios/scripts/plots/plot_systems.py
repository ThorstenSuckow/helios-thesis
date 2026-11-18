"""10a, 10b and 10c: bar charts of the system, manager and collision phase times.

One figure per plot type; the load scenarios (n) are stacked as rows (top to bottom in the order of ns),
every row has its own y scale.

Data:
    10a  system_means.csv, update_* of m_col, m_lft, s_beta, s_lambda_prt (without flush); one column
    10b  collision_phase_means.csv, total_* of the phases det and res (with flush); columns Erkennung, Behandlung
    10c  system_means.csv, update_* of s_delta_det_* / s_delta_res_* (without flush); columns Erkennung, Behandlung
Error bars: minimum to maximum of the run means.

plot_systems() returns the written paths and the bars it drew (mean, minimum, maximum, value text) so that
they can be checked against the CSVs (see check_plot_values.py).
"""

from __future__ import annotations

import sys
from pathlib import Path

import matplotlib
import matplotlib.patches
import matplotlib.pyplot as plt
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
import plot_style  # noqa: E402
from plot_common import color_of, format_us, runs_text, vname, wrap_math  # noqa: E402
from plot_style import PRINT_WIDTH, mark_values, save_figure  # noqa: E402

# Systems that treat the domains Go and Prt together: the *_go series holds the whole system (Go + Prt).
COMBINED_SYSTEMS_BY_VARIANT = {
    "v1": ["s_delta_det", "s_delta_res"],
    "v2": ["s_delta_det", "s_delta_res"],
    "v3": [],
}

SYSTEM_NOTATION = ("")
COMMON_LABELS = {"m_col": r"$m_{\delta}$", "m_lft": r"$m_{\lambda}$", "s_beta": r"$s_{\beta}$", "s_lambda_prt": r"$s_{\lambda}$"}
GROUP_TEXT = ("")
SYSTEM_TEXT = SYSTEM_NOTATION.replace("\n", " ")
ERRORBAR_TEXT = "Fehlerbalken: Minimum bis Maximum der Run-Mittelwerte."

# Figure height: the figure plus a caption of about 4 lines must fit on one page. \textheight of the thesis
# (class report, 11pt, a4paper, no geometry package) = 595.8 pt; the figure is at most 80 % of it.
TEXTHEIGHT_PT = 595.8
MAX_FIGURE_HEIGHT = 0.8 * TEXTHEIGHT_PT / 72.27   # inch
ROW_HEIGHT = 1.30      # inch, height of the axes of one row (n)
ROW_GAP = 0.40         # inch between two rows (tick labels)
YLABEL = "Mittelwert der Ausführungszeit [µs]"


def combined_systems(variant: str) -> list[str]:
    return COMBINED_SYSTEMS_BY_VARIANT.get(variant, [])


def collision_key(variant: str, base: str, kind: str) -> str | None:
    """V1/V2: combined system (series *_go) only as 'gesamt'; V3/V5: separate for Go and Prt."""
    if base in combined_systems(variant):
        return f"{base}_go" if kind == "all" else None
    return None if kind == "all" else f"{base}_{kind}"


def _fit_values(fig, ax, texts, legend, factors, top: float) -> bool:
    """Chooses the smallest y upper limit (top * factor) at which value texts cover neither each other nor the legend."""
    renderer = None
    for factor in factors:
        ax.set_ylim(0, top * factor)
        fig.canvas.draw()
        renderer = fig.canvas.get_renderer()
        if not texts:
            return True
        ok = not plot_style.text_overlaps(texts, renderer)
        axes_box = ax.get_window_extent(renderer)
        if ok and legend is not None:
            legend_box = legend.get_window_extent(renderer)
            ok = not any(t.get_window_extent(renderer).overlaps(legend_box) for t in texts)
        ok = ok and all(t.get_window_extent(renderer).y1 <= axes_box.y1 for t in texts)
        if ok:
            return True
    for t in texts:
        t.remove()
    ax.set_ylim(0, top * factors[1])
    return False


class Values(dict):
    """Mean per variant; .ranges: (minimum, maximum) of the run means per variant; .keys: CSV row key."""

    def __init__(self):
        super().__init__()
        self.ranges: dict[str, tuple[float, float]] = {}
        self.keys: dict[str, str] = {}


def _split_panel(fig, ax, title: str | None, items: list[tuple[str, dict]], variants: list[str], *,
                 centered: bool, rotate: bool, span: float = 0.8) -> list[dict]:
    """Grouped bars; value texts (horizontal or rotated by 90 degrees) only if they fit without overlap.

    Returns one record per bar: item label, variant, key, mean, minimum, maximum and the value text artist.
    """
    width = span / len(variants)
    texts = []
    records: list[dict] = []
    for i, (label, values) in enumerate(items):
        present = [v for v in variants if v in values]
        for j, v in enumerate(present if centered else variants):
            if v not in values:
                continue
            offset = (j - (len(present) - 1) / 2) * width if centered else (j - (len(variants) - 1) / 2) * width
            ax.bar(i + offset, values[v], width=width * 0.95, color=color_of(v, variants))
            low, high = getattr(values, "ranges", {}).get(v, (values[v], values[v]))
            ax.errorbar(i + offset, values[v], yerr=[[values[v] - low], [high - values[v]]], fmt="none",
                        ecolor="black", elinewidth=0.6, capsize=1.5, capthick=0.6, zorder=4)
            text = ax.annotate(format_us(values[v]), xy=(i + offset, max(high, values[v])), xytext=(0, 2),
                               textcoords="offset points", ha="center", va="bottom", fontsize=7,
                               rotation=90 if rotate else 0)
            texts.append(text)
            records.append({"panel": title, "item": label, "variant": v, "key": getattr(values, "keys", {}).get(v),
                            "mean": values[v], "min": low, "max": high, "artist": text, "axes": ax})
    mark_values(texts)
    ax.set_xticks(range(len(items)))
    ax.set_xticklabels([label for label, _ in items], fontsize=8)
    ax.set_xlim(-0.5 if len(items) == 1 else -0.6, (0.5 if len(items) == 1 else len(items) - 0.4))
    ax.tick_params(axis="y", labelsize=8)
    ax.grid(alpha=0.3, axis="y")
    if title:
        ax.set_title(title, fontsize=9)
    top = max((getattr(values, "ranges", {}).get(v, (0, x))[1] for _, values in items for v, x in values.items()),
              default=1.0) or 1.0
    _fit_values(fig, ax, texts, None, (1.55, 1.9, 2.4) if rotate else (1.3, 1.5, 1.8), top)
    return records


def _n_label(n: int) -> str:
    """n in the notation of the thesis: $n = 1\\,024$."""
    return f"$n = {n:,}$".replace(",", r"\,")


def _stack_figure(path: Path, title: str, rows: list[tuple[int, list]], variants: list[str], footer: str, *,
                  centered: bool, rotate: bool, span: float = 0.8) -> list[dict]:
    """One figure, one row per n (own y scale), one column per panel; legend on top, footer at the bottom."""
    footer = footer + "\n" + ERRORBAR_TEXT
    lines = footer.count("\n") + 1
    ncols = max(len(panels) for _, panels in rows)
    column_titles = any(panel_title for panel_title, _ in rows[0][1])
    top_in = 0.30 + 0.27 + (0.22 if column_titles else 0.0)     # suptitle, legend, column titles
    bottom_in = 0.42 + 0.125 * lines
    height = top_in + bottom_in + len(rows) * ROW_HEIGHT + (len(rows) - 1) * ROW_GAP
    fig = plt.figure(figsize=(PRINT_WIDTH, height))
    gs = fig.add_gridspec(len(rows), ncols, left=0.17, right=0.985, bottom=bottom_in / height,
                          top=1 - top_in / height, wspace=0.45, hspace=ROW_GAP / ROW_HEIGHT)
    fig.suptitle(title, y=0.99, fontsize=10)
    handles = [matplotlib.patches.Patch(color=color_of(v, variants), label=vname(v)) for v in variants]
    fig.legend(handles=handles, loc="upper center", bbox_to_anchor=(0.5, 1 - 0.30 / height), ncols=len(handles),
               frameon=False, fontsize=8, handlelength=1.0, columnspacing=0.8, handletextpad=0.4)
    records: list[dict] = []
    for r, (n, panels) in enumerate(rows):
        for c, (panel_title, items) in enumerate(panels):
            ax = fig.add_subplot(gs[r, c])
            for rec in _split_panel(fig, ax, panel_title if r == 0 else None, items, variants,
                                    centered=centered, rotate=rotate, span=span):
                rec.update(n=n, panel=panel_title)
                records.append(rec)
            if c == 0:
                pos = ax.get_position()
                fig.text(0.075, (pos.y0 + pos.y1) / 2, _n_label(n), rotation=90, ha="center", va="center",
                         fontsize=10)
    fig.supylabel(YLABEL, x=0.012, fontsize=8)
    fig.text(0.01, 0.008, footer, ha="left", va="bottom", fontsize=7)
    save_figure(fig, path)
    for rec in records:
        artist = rec.pop("artist")
        axes = rec.pop("axes")
        rec["text"] = artist.get_text() if artist in axes.texts else None
        rec["plot"] = path.stem
    return records



def _values(frame: pd.DataFrame, variants: list[str], key_of, prefix: str, columns: tuple[str, str, str]) -> Values:
    """Values (mean, range of the run means) per variant from the rows of one n; key_of(variant) -> row key or None."""
    values = Values()
    mean_col, min_col, max_col = columns
    for v in variants:
        key = key_of(v)
        if key is None:
            continue
        row = frame[(frame["variant"] == v) & (frame[prefix] == key)]
        if row.empty or pd.isna(row.iloc[0][mean_col]):
            continue
        values[v] = float(row.iloc[0][mean_col])
        values.ranges[v] = (float(row.iloc[0][min_col]), float(row.iloc[0][max_col]))
        values.keys[v] = key
    return values


def plot_systems(system_means: pd.DataFrame, phase_means: pd.DataFrame, ns: list[int],
                 out_dir: Path) -> tuple[list[Path], list[dict]]:
    """Create 10a, 10b and 10c (one figure each, one row per n in ns).

    Returns the written paths and one record per drawn bar: plot, n, panel, item, variant, key, mean, min, max
    (all in µs, as drawn) and text (the value text, None if it was dropped for lack of space).
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    upd = ("update_mean_us", "update_min_run_mean_us", "update_max_run_mean_us")
    tot = ("total_mean_us", "total_min_run_mean_us", "total_max_run_mean_us")
    for n in ns:
        if system_means[system_means["n"] == n].empty or phase_means[phase_means["n"] == n].empty:
            raise ValueError(f"n = {n} is not part of system_means.csv / collision_phase_means.csv")
    sys_sel = system_means[system_means["n"].isin(ns)]
    phase_sel = phase_means[phase_means["n"].isin(ns)]
    variants = sorted(sys_sel["variant"].unique())
    phase_variants = sorted(phase_sel["variant"].unique())
    runs_line = "Mittelwert der Run-Mittelwerte über " + runs_text(list(sys_sel["runs"]))
    phase_runs_line = "Mittelwert der Run-Mittelwerte über " + runs_text(list(phase_sel["runs"]))

    rows_a, rows_b, rows_c = [], [], []
    for n in ns:
        sys_n = system_means[system_means["n"] == n]
        phase_n = phase_means[phase_means["n"] == n]

        # 10a: update() without flush()
        common = [(COMMON_LABELS[k], _values(sys_n, variants, lambda v, k=k: k, "system", upd))
                  for k in ("m_col", "m_lft", "s_beta", "s_lambda_prt")]
        common = [(label, values) for label, values in common if values]
        if common:
            rows_a.append((n, [(None, common)]))

        # 10b: collision phases with flush(), one panel per phase
        panels = []
        for key, label in (("det", "Erkennung"), ("res", "Behandlung")):
            values = _values(phase_n, phase_variants, lambda v, key=key: key, "phase", tot)
            if values:
                panels.append((label, [(label, values)]))
        if panels:
            rows_b.append((n, panels))

        # 10c: collision systems, update() without flush()
        panels = []
        for base, label in (("s_delta_det", "Erkennung"), ("s_delta_res", "Behandlung")):
            items = []
            for kind, text in (("all", "gesamt"), ("go", "Go"), ("prt", "Prt")):
                values = _values(sys_n, variants, lambda v, base=base, kind=kind: collision_key(v, base, kind),
                                 "system", upd)
                if values:
                    items.append((text, values))
            if items:
                panels.append((label, items))
        if panels:
            rows_c.append((n, panels))

    written: list[Path] = []
    bars: list[dict] = []
    if rows_a:
        path = out_dir / "10a_gemeinsame_systeme.png"
        bars += _stack_figure(path, "Gemeinsame Systeme und Manager", rows_a, variants, runs_line,
                              centered=False, rotate=False)
        written.append(path)
    if rows_b:
        path = out_dir / "10b_kollisionsverarbeitung.png"
        bars += _stack_figure(path, "Kollisionsverarbeitung inkl. flush()", rows_b, phase_variants,
                              wrap_math(GROUP_TEXT, 118) + "\n" + phase_runs_line,
                              centered=True, rotate=False, span=0.9)
        written.append(path)
    if rows_c:
        notation = wrap_math(SYSTEM_TEXT.replace(r"\mathcal{D}", "D"), 118)
        path = out_dir / "10c_kollisionssysteme.png"
        bars += _stack_figure(path, "Kollisionssysteme je Variante (update())", rows_c, variants,
                              notation + "\n" + runs_line, centered=True, rotate=True)
        written.append(path)
    return written, bars

