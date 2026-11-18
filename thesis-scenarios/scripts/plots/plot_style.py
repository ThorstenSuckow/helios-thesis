"""Gemeinsame Darstellungsparameter fuer die Druckgroesse der Arbeit (Textbreite ~ 16 cm = 6,3 Zoll).

- apply_style(): zentrale rcParams (Schrift, Linienbreite, Markergroesse)
- print_size(height): Abbildungsgroesse (Breite 6,3 Zoll, Hoehe hoechstens 9,0 Zoll)
- save_figure(fig, path): einheitliches Speichern (dpi 300, bbox tight, Format als Parameter/Standard "png"),
  verwirft Wertebeschriftungen (gid="value"), die sich bei dieser Groesse ueberlappen, und protokolliert
  Textueberlappungen je Datei (REPORT).
"""

from __future__ import annotations

import os
from pathlib import Path

import matplotlib
import matplotlib.pyplot as plt
from matplotlib.text import Text
from matplotlib.transforms import Bbox

PRINT_WIDTH = 6.3       # Zoll, entspricht width=\textwidth
SINGLE_SIZE = (6.3, 3.4)  # Einzelplots
MAX_HEIGHT = 9.0        # Zoll, mehrteilige Plots
VALUE_FONT_SIZE = 7     # Hoechstgroesse der Wertebeschriftungen an Punkten/Balken
SAVE_DPI = 300
OUTPUT_FORMAT = os.environ.get("HELIOS_PLOT_FORMAT", "png")  # spaeter z. B. "pdf": betrifft nur die Dateiendung

# Je gespeicherter Datei: (Pfad, Zahl ueberlappender Textpaare, Beispiele, Wertebeschriftungen entfallen?)
REPORT: list[tuple[str, int, list[str], bool]] = []
# Dateinamen (ohne Endung), bei denen Wertebeschriftungen aus Platzgruenden entfallen sind
DROPPED_VALUES: set[str] = set()


def apply_style() -> None:
    matplotlib.rcParams.update({
        "font.size": 9,
        "axes.titlesize": 10,
        "axes.labelsize": 9,
        "legend.fontsize": 8,
        "xtick.labelsize": 8,
        "ytick.labelsize": 8,
        "figure.titlesize": 10,
        "lines.linewidth": 1.2,
        "lines.markersize": 4,
        "savefig.dpi": SAVE_DPI,
    })


def print_size(height: float | None = None) -> tuple[float, float]:
    """Abbildungsgroesse in Zoll: Breite 6,3; ohne Hoehe Einzelplot (6,3 x 3,4), sonst hoechstens 9,0."""
    if height is None:
        return SINGLE_SIZE
    return PRINT_WIDTH, min(float(height), MAX_HEIGHT)


def _skipped_tick_labels(fig) -> set[int]:
    """Tick-Beschriftungen ausserhalb des sichtbaren Bereichs (werden nicht gezeichnet)."""
    skip: set[int] = set()
    for ax in fig.axes:
        for axis in (ax.xaxis, ax.yaxis):
            low, high = sorted(axis.get_view_interval())
            for tick in axis.get_major_ticks():
                loc = tick.get_loc()
                if not (low - 1e-9 * abs(high - low) <= loc <= high + 1e-9 * abs(high - low)):
                    skip.add(id(tick.label1))
                    skip.add(id(tick.label2))
    return skip


def _visible_texts(fig) -> list[Text]:
    fig.canvas.draw()
    skip = _skipped_tick_labels(fig)
    texts = []
    for t in fig.findobj(Text):
        if id(t) in skip or not t.get_visible() or not t.get_text().strip():
            continue
        if t.axes is not None and not t.axes.get_visible():
            continue
        texts.append(t)
    return texts


def text_overlaps(texts: list[Text], renderer, pad: float = 0.5) -> list[tuple[Text, Text]]:
    boxes = []
    for t in texts:
        b = t.get_window_extent(renderer)
        boxes.append(Bbox.from_extents(b.x0 + pad, b.y0 + pad, b.x1 - pad, b.y1 - pad))
    return [(texts[i], texts[j]) for i in range(len(texts)) for j in range(i + 1, len(texts))
            if boxes[i].overlaps(boxes[j])]


def drop_overlapping_values(fig) -> bool:
    """Entfernt alle Wertebeschriftungen (gid == "value") der Abbildung, wenn sich zwei davon ueberlappen."""
    values = [t for t in fig.findobj(Text) if t.get_gid() == "value" and t.get_text().strip()]
    if len(values) < 2:
        return False
    fig.canvas.draw()
    renderer = fig.canvas.get_renderer()
    if not text_overlaps(values, renderer):
        return False
    for t in values:
        t.remove()
    return True


def mark_values(artists) -> None:
    """Markiert Wertebeschriftungen (Text, Annotation oder Liste davon), Schrift hoechstens VALUE_FONT_SIZE."""
    for t in artists if isinstance(artists, (list, tuple)) else [artists]:
        t.set_gid("value")
        if t.get_fontsize() > VALUE_FONT_SIZE:
            t.set_fontsize(VALUE_FONT_SIZE)


def save_figure(fig, path: Path, fmt: str | None = None, *, check: bool = True) -> Path:
    """Speichert fig; die Dateiendung folgt dem Format (Standard OUTPUT_FORMAT)."""
    fmt = fmt or OUTPUT_FORMAT
    path = Path(path).with_suffix(f".{fmt}")
    dropped = drop_overlapping_values(fig) or path.stem in DROPPED_VALUES
    if check:
        renderer = fig.canvas.get_renderer()
        pairs = text_overlaps(_visible_texts(fig), renderer)
        examples = [f"'{a.get_text()[:25]}' x '{b.get_text()[:25]}'" for a, b in pairs[:3]]
        tight = fig.get_tightbbox(renderer)
        if tight.width > PRINT_WIDTH + 0.1 or tight.height > MAX_HEIGHT + 0.1:
            examples.append(f"Groesse {tight.width:.2f} x {tight.height:.2f} Zoll")
            pairs = pairs or [None]
        REPORT.append((str(path), len(pairs), examples, dropped))
    fig.savefig(path, dpi=SAVE_DPI, bbox_inches="tight", pad_inches=0.02, format=fmt)
    plt.close(fig)
    return path


def print_report() -> None:
    """Zusammenfassung der Lesbarkeitspruefung (Textueberlappungen, entfallene Wertebeschriftungen)."""
    bad = [r for r in REPORT if r[1]]
    dropped = [r for r in REPORT if r[3]]
    print(f"\nLesbarkeitspruefung: {len(REPORT)} Abbildungen, {len(bad)} mit ueberlappenden Texten, "
          f"{len(dropped)} ohne Wertebeschriftungen (Platz)")
    for path, count, examples, _ in bad:
        print(f"  UEBERLAPPUNG {count}x in {Path(path).name}: " + "; ".join(examples))
    for path, *_ in dropped:
        print(f"  Werte entfallen (siehe CSV): {Path(path).name}")


